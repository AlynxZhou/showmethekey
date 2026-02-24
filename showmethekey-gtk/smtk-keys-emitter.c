#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <grp.h>

#include "config.h"
#include "smtk-enum-types.h"
#include "smtk-keys-emitter.h"
#include "smtk-keys-mapper.h"
#include "smtk-event.h"

#define MAX_KEYS 30

struct _SmtkKeysEmitter {
	GObject parent_instance;
	GSettings *settings;
	SmtkKeysMapper *mapper;
	GSubprocess *cli;
	GDataInputStream *cli_out;

	GThread *poller;
	bool polling;

	SmtkKeyMode mode;
	bool show_keyboard;
	bool show_mouse;
	bool ctrl_pressed;
	bool alt_pressed;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysEmitter, smtk_keys_emitter, G_TYPE_OBJECT)

enum { SIG_KEY, SIG_ERROR_CLI_EXIT, N_SIGNALS };

static unsigned int sigs[N_SIGNALS] = { 0 };

enum { PROP_0, PROP_MODE, PROP_SHOW_KEYBOARD, PROP_SHOW_MOUSE, N_PROPS };

static GParamSpec *props[N_PROPS] = { NULL };

// Check whether user choose cancel for pkexec.
static void
on_cli_complete(GObject *source_object, GAsyncResult *res, void *data)
{
	// We got a copy of SmtkKeysEmitter's address when setting up this
	// callback, and there is a condition, that user closes the switch,
	// and emitter is disposed, then this callback is called, and the
	// address we hold is invalid, it might point to other objects, and
	// still not NULL. To solve this problem we hold a reference to this
	// callback to prevent the emitter to be disposed and manually drop it.
	g_debug("Calling on_cli_complete().");

	g_autoptr(SmtkKeysEmitter) this = data;
	// Cli may already released normally, and this function only cares about
	// when cli exited with error, for example user cancelled pkexec.
	if (this->cli == NULL || g_subprocess_get_exit_status(this->cli) == 0)
		return;

	// Better to close thread here to prevent a lot of error.
	this->polling = false;
	if (this->poller != NULL) {
		g_debug("Stopping poller because cli exitted.");
		g_thread_join(this->poller);
		this->poller = NULL;
	}
	g_signal_emit_by_name(this, "error-cli-exit");
}

static void set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeysEmitter *this = SMTK_KEYS_EMITTER(o);

	switch (prop) {
	case PROP_MODE:
		this->mode = g_value_get_enum(value);
		break;
	case PROP_SHOW_KEYBOARD:
		this->show_keyboard = g_value_get_boolean(value);
		break;
	case PROP_SHOW_MOUSE:
		this->show_mouse = g_value_get_boolean(value);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void
get_property(GObject *o, unsigned int prop, GValue *value, GParamSpec *pspec)
{
	SmtkKeysEmitter *this = SMTK_KEYS_EMITTER(o);

	switch (prop) {
	case PROP_MODE:
		g_value_set_enum(value, this->mode);
		break;
	case PROP_SHOW_KEYBOARD:
		g_value_set_boolean(value, this->show_keyboard);
		break;
	case PROP_SHOW_MOUSE:
		g_value_set_boolean(value, this->show_mouse);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

struct key_idle_data {
	SmtkKeysEmitter *emitter;
	char *key;
};

// true and false are C99 _Bool, but GLib expects gboolean, which is C99 int.
static int key_idle(void *data)
{
	// Here we back to UI thread.
	g_autofree struct key_idle_data *key_idle_data = data;
	g_autoptr(SmtkKeysEmitter) this = key_idle_data->emitter;
	g_autofree char *key = key_idle_data->key;

	g_signal_emit_by_name(this, "key", key);

	return 0;
}

static void trigger_key_idle(SmtkKeysEmitter *this, const char key[])
{
	// UI can only be modified in UI thread, and we are not in UI thread
	// here. So we need to use `g_timeout_add()` to kick an async callback
	// into glib's main loop (the same as GTK UI thread).
	//
	// Signals are not async! So we cannot emit signal here, because they
	// will run in poller thread instead of UI thread. `g_idle_add()` is not
	// suitable because we have a high priority. We dup the key and use
	// malloc because we will enter another thread, so the poller thread may
	// already continue and calls free for key.
	struct key_idle_data *key_idle_data = g_malloc0(sizeof(*key_idle_data));
	key_idle_data->emitter = g_object_ref(this);
	key_idle_data->key = g_strdup(key);
	g_timeout_add_full(G_PRIORITY_DEFAULT, 0, key_idle, key_idle_data, NULL);
}

static int toggle_clickable(void *data)
{
	SmtkKeysEmitter *this = data;

	const char *key = "clickable";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);

	return 0;
}

static void trigger_clickable_idle(SmtkKeysEmitter *this)
{
	g_timeout_add_full(
		G_PRIORITY_DEFAULT,
		0,
		toggle_clickable,
		g_object_ref(this),
		g_object_unref
	);
}

static int toggle_paused(void *data)
{
	SmtkKeysEmitter *this = data;

	const char *key = "paused";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);

	return 0;
}

static void trigger_paused_idle(SmtkKeysEmitter *this)
{
	g_timeout_add_full(
		G_PRIORITY_DEFAULT,
		0,
		toggle_paused,
		g_object_ref(this),
		g_object_unref
	);
}

static void *poll_cli(void *data)
{
	SmtkKeysEmitter *this = data;
	while (this->polling) {
		g_autoptr(GError) error = NULL;
		g_autofree char *line = g_data_input_stream_read_line(
			this->cli_out, NULL, NULL, &error
		);
		// See <https://developer.gnome.org/gio/2.56/GDataInputStream.html#g-data-input-stream-read-line>.
		if (line == NULL) {
			if (error != NULL)
				g_warning(
					"Read line error: %s.", error->message
				);
			continue;
		}

		g_autoptr(SmtkEvent) event = smtk_event_new(line);
		if (event == NULL)
			continue;

		// Quickly press Ctrl twice or both Ctrl to toggle clickable and
		// quickly press Alt twice or both Alt to toggle paused.
		// Because xkbcommon treat left and right alt as the same one, we
		// have to do it here, not in mapper.
		SmtkEventType type = event->type;
		SmtkEventState state = event->state;
		const char *key_name = event->key_name;
		if (type == SMTK_EVENT_TYPE_KEYBOARD_KEY) {
			if (state == SMTK_EVENT_STATE_PRESSED) {
				if (g_strcmp0(key_name, "KEY_LEFTCTRL") == 0 ||
				    g_strcmp0(key_name, "KEY_RIGHTCTRL") == 0) {
					if (this->ctrl_pressed) {
						trigger_clickable_idle(this);
						this->ctrl_pressed = false;
					} else {
						this->ctrl_pressed = true;
					}
				}
				if (g_strcmp0(key_name, "KEY_LEFTALT") == 0 ||
				    g_strcmp0(key_name, "KEY_RIGHTALT") == 0) {
					if (this->alt_pressed) {
						trigger_paused_idle(this);
						this->alt_pressed = false;
					} else {
						this->alt_pressed = true;
					}
				}
			}
		} else {
			if (this->ctrl_pressed)
				this->ctrl_pressed = false;
			if (this->alt_pressed)
				this->alt_pressed = false;
		}
		g_autofree char *key = NULL;
		// Always get key with SmtkKeysMapper, it will update XKB state
		// to keep sync with actual keyboard.
		switch (this->mode) {
		case SMTK_KEY_MODE_COMPOSED:
			key = smtk_keys_mapper_get_composed(this->mapper, event);
			break;
		case SMTK_KEY_MODE_RAW:
			key = smtk_keys_mapper_get_raw(this->mapper, event);
			break;
		case SMTK_KEY_MODE_COMPACT:
			key = smtk_keys_mapper_get_compact(this->mapper, event);
			break;
		default:
			// Should never be here.
			g_warn_if_reached();
			break;
		}
		if (key != NULL) {
			if (state == SMTK_EVENT_STATE_PRESSED &&
			    ((this->show_mouse &&
			      type == SMTK_EVENT_TYPE_POINTER_BUTTON) ||
			     (this->show_keyboard &&
			      type == SMTK_EVENT_TYPE_KEYBOARD_KEY)))
				trigger_key_idle(this, key);
		}
	}
	return NULL;
}

static void constructed(GObject *o)
{
	// Seems we can only get constructor properties here.
	SmtkKeysEmitter *this = SMTK_KEYS_EMITTER(o);

	this->mapper = smtk_keys_mapper_new(&this->error);
	// `this->error` is already set, just return.
	if (this->mapper == NULL)
		goto out;

	this->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(
		this->settings, "mode", this, "mode", G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"show-keyboard",
		this,
		"show-keyboard",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"show-mouse",
		this,
		"show-mouse",
		G_SETTINGS_BIND_GET
	);

out:
	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->constructed(o);
}

static void dispose(GObject *o)
{
	SmtkKeysEmitter *this = SMTK_KEYS_EMITTER(o);

	g_clear_object(&this->settings);
	g_clear_object(&this->mapper);

	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->dispose(o);
}

// static void finalize(GObject *o)
// {
// 	SmtkKeysEmitter *this = SMTK_KEYS_EMITTER(o);

// 	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->finalize(o);
// }

static void smtk_keys_emitter_class_init(SmtkKeysEmitterClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);

	o_class->set_property = set_property;
	o_class->get_property = get_property;

	o_class->constructed = constructed;

	o_class->dispose = dispose;
	// o_class->finalize = finalize;

	sigs[SIG_KEY] = g_signal_new(
		"key",
		SMTK_TYPE_KEYS_EMITTER,
		G_SIGNAL_RUN_LAST,
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING
	);
	sigs[SIG_ERROR_CLI_EXIT] = g_signal_new(
		"error-cli-exit",
		SMTK_TYPE_KEYS_EMITTER,
		G_SIGNAL_RUN_LAST,
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	props[PROP_MODE] = g_param_spec_enum(
		"mode",
		"Mode",
		"Key Mode",
		SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_SHOW_KEYBOARD] = g_param_spec_boolean(
		"show-keyboard",
		"Show Keyboard",
		"Show Keyboard Key",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse",
		"Show Mouse",
		"Show Mouse Button",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_keys_emitter_init(SmtkKeysEmitter *this)
{
	this->settings = NULL;
	this->mapper = NULL;
	this->cli = NULL;
	this->cli_out = NULL;
	this->poller = NULL;
	this->error = NULL;
	this->show_keyboard = true;
	this->show_mouse = true;
	this->ctrl_pressed = false;
	this->alt_pressed = false;
}

SmtkKeysEmitter *smtk_keys_emitter_new(GError **error)
{
	SmtkKeysEmitter *this = g_object_new(SMTK_TYPE_KEYS_EMITTER, NULL);

	if (this->error != NULL) {
		g_propagate_error(error, this->error);
		g_object_unref(this);
		return NULL;
	}

	return this;
}

static bool is_group(const char *group_name)
{
	g_autofree gid_t *groups = NULL;
	int ngroups;
	struct group *grp;
	gid_t gid;

	ngroups = getgroups(0, NULL);
	groups = g_malloc0(ngroups * sizeof(*groups));
	if (getgroups(ngroups, groups) < 0)
		return false;

	grp = getgrnam(group_name);
	if (!grp)
		return false;
	gid = grp->gr_gid;

	for (int i = 0; i < ngroups; i++)
		if (groups[i] == gid)
			return true;

	return false;
}

// Those two functions are splitted from init and dispose functions,
// because we need to pass a reference to the async callback of GTask,
// and don't want a loop reference (e.g. a emitter reference is dropped
// only when callback is called, and callback is called only when cli stopped,
// but cli is stopped only when emitter is disposed, and emitter is disposed
// only when reference is dropped!). So we break it into different functions
// and let the caller stop the cli before dispose.
void smtk_keys_emitter_start_async(SmtkKeysEmitter *this, GError **error)
{
	g_debug("Calling smtk_keys_emitter_start_async().");
	g_return_if_fail(this != NULL);

	if (is_group("input"))
		this->cli = g_subprocess_new(
			G_SUBPROCESS_FLAGS_STDIN_PIPE |
				G_SUBPROCESS_FLAGS_STDOUT_PIPE |
				G_SUBPROCESS_FLAGS_STDERR_PIPE,
			error,
			PACKAGE_BINDIR "/showmethekey-cli",
			NULL
		);
	else
		this->cli = g_subprocess_new(
			G_SUBPROCESS_FLAGS_STDIN_PIPE |
				G_SUBPROCESS_FLAGS_STDOUT_PIPE |
				G_SUBPROCESS_FLAGS_STDERR_PIPE,
			error,
			PKEXEC_PATH,
			PACKAGE_BINDIR "/showmethekey-cli",
			NULL
		);
	// this->error is already set, just return.
	if (this->cli == NULL)
		return;
	// Actually I don't wait the subprocess to return, they work like
	// clients and daemons, why clients want to wait for daemons' exiting?
	// This is just spawn subprocess.
	// smtk_keys_emitter_cli_on_complete is called when GTask finished,
	// and this is async and might the SmtkKeysWin is already destroyed.
	// So we have to manually reference to emitter here.
	g_subprocess_wait_check_async(
		this->cli, NULL, on_cli_complete, g_object_ref(this)
	);
	this->cli_out = g_data_input_stream_new(
		g_subprocess_get_stdout_pipe(this->cli)
	);

	this->polling = true;
	this->poller = g_thread_try_new("poller", poll_cli, this, error);
	// this->error is already set, just return.
	if (this->poller == NULL)
		return;
}

void smtk_keys_emitter_stop_async(SmtkKeysEmitter *this)
{
	g_debug("Calling smtk_keys_emitter_stop_async().");
	g_return_if_fail(this != NULL);

	// Don't know why but I need to stop cli before poller.
	if (this->cli != NULL) {
		// Because we run subprocess with pkexec,
		// so we cannot force kill it,
		// we use stdin pipe to write a "stop\n",
		// and let it exit by itself.
		const char stop[] = "stop\n";
		GBytes *input = g_bytes_new(stop, sizeof(stop));
		g_subprocess_communicate(
			this->cli, input, NULL, NULL, NULL, NULL
		);
		g_bytes_unref(input);
		// g_subprocess_force_exit(this->cli);
		// Just close it, I am not interested in its error.
		g_input_stream_close(G_INPUT_STREAM(this->cli_out), NULL, NULL);
		this->cli_out = NULL;
		g_object_unref(this->cli);
		this->cli = NULL;
	}

	if (this->poller != NULL) {
		this->polling = false;
		// This will wait until thread exited.
		// It will call g_thread_unref() internal
		// so we don't need to do it.
		g_thread_join(this->poller);
		// g_thread_unref(this->poller);
		this->poller = NULL;
	}
}
