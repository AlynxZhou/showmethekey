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
	bool alt_pressed;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysEmitter, smtk_keys_emitter, G_TYPE_OBJECT)

enum { SIG_KEY, SIG_ERROR_CLI_EXIT, N_SIGNALS };

static unsigned int obj_signals[N_SIGNALS] = { 0 };

enum { PROP_0, PROP_MODE, PROP_SHOW_KEYBOARD, PROP_SHOW_MOUSE, N_PROPS };

static GParamSpec *obj_props[N_PROPS] = { NULL };

// Check whether user choose cancel for pkexec.
static void smtk_keys_emitter_cli_on_complete(GObject *source_object,
					      GAsyncResult *res, void *data)
{
	// We got a copy of SmtkKeysEmitter's address when setting up this
	// callback, and there is a condition, that user closes the switch,
	// and emitter is disposed, then this callback is called, and the
	// address we hold is invalid, it might point to other objects, and
	// still not NULL. To solve this problem we hold a reference to this
	// callback to prevent the emitter to be disposed and manually drop it.
	g_debug("Calling smtk_keys_emitter_cli_on_complete().");

	SmtkKeysEmitter *emitter = data;
	// Cli may already released normally, and this function only cares about
	// when cli exited with error, for example user cancelled pkexec.
	if (emitter->cli != NULL &&
	    g_subprocess_get_exit_status(emitter->cli) != 0) {
		// Better to close thread here to prevent a lot of error.
		emitter->polling = false;
		if (emitter->poller != NULL) {
			g_debug("Stopping poller because cli exitted.");
			g_thread_join(emitter->poller);
			emitter->poller = NULL;
		}
		g_signal_emit_by_name(emitter, "error-cli-exit");
	}
	g_object_unref(emitter);
}

static void smtk_keys_emitter_set_property(GObject *object,
					   unsigned int property_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	switch (property_id) {
	case PROP_MODE:
		emitter->mode = g_value_get_enum(value);
		break;
	case PROP_SHOW_KEYBOARD:
		emitter->show_keyboard = g_value_get_boolean(value);
		break;
	case PROP_SHOW_MOUSE:
		emitter->show_mouse = g_value_get_boolean(value);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_emitter_get_property(GObject *object,
					   unsigned int property_id,
					   GValue *value, GParamSpec *pspec)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	switch (property_id) {
	case PROP_MODE:
		g_value_set_enum(value, emitter->mode);
		break;
	case PROP_SHOW_KEYBOARD:
		g_value_set_boolean(value, emitter->show_keyboard);
		break;
	case PROP_SHOW_MOUSE:
		g_value_set_boolean(value, emitter->show_mouse);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

struct key_idle_data {
	SmtkKeysEmitter *emitter;
	char *key;
};

static void key_idle_destroy_function(void *data)
{
	struct key_idle_data *key_idle_data = data;
	SmtkKeysEmitter *emitter = key_idle_data->emitter;

	g_object_unref(emitter);
	g_free(key_idle_data);
}

// true and false are C99 _Bool, but GLib expects gboolean, which is C99 int.
static int key_idle_function(void *data)
{
	// Here we back to UI thread.
	struct key_idle_data *key_idle_data = data;
	SmtkKeysEmitter *emitter = key_idle_data->emitter;

	g_signal_emit_by_name(emitter, "key", key_idle_data->key);

	return 0;
}

static void trigger_key_idle_function(SmtkKeysEmitter *emitter,
				      const char key[])
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
	struct key_idle_data *key_idle_data = g_malloc(sizeof(*key_idle_data));
	if (!key_idle_data) {
		g_warning("Alloc key_idle_data failed.\n");
		return;
	}
	key_idle_data->emitter = g_object_ref(emitter);
	key_idle_data->key = g_strdup(key);
	g_timeout_add_full(G_PRIORITY_DEFAULT, 0, key_idle_function,
			   key_idle_data, key_idle_destroy_function);
}

static int pause_idle_function(void *data)
{
	SmtkKeysEmitter *emitter = data;

	g_settings_set_boolean(emitter->settings, "paused",
			       !g_settings_get_boolean(emitter->settings,
						       "paused"));

	return 0;
}

static void trigger_pause_idle_function(SmtkKeysEmitter *emitter)
{
	g_timeout_add_full(G_PRIORITY_DEFAULT, 0, pause_idle_function,
			   g_object_ref(emitter), g_object_unref);
}

static void *poller_function(void *data)
{
	SmtkKeysEmitter *emitter = data;
	while (emitter->polling) {
		GError *read_line_error = NULL;
		char *line = g_data_input_stream_read_line(
			emitter->cli_out, NULL, NULL, &read_line_error);
		// See <https://developer.gnome.org/gio/2.56/GDataInputStream.html#g-data-input-stream-read-line>.
		if (line == NULL) {
			if (read_line_error != NULL) {
				g_warning("Read line error: %s.",
					  read_line_error->message);
				g_error_free(read_line_error);
			}
			continue;
		}

		GError *event_error = NULL;
		SmtkEvent *event = smtk_event_new(line, &event_error);
		if (event == NULL) {
			g_warning("Create event error: %s.",
				  event_error->message);
			g_free(line);
			continue;
		}

		// TODO: Press both Ctrl to clickable?

		// Press both Alt to pause. Xkbcommon treat left and right alt
		// as the same one, so we have to do it here.
		SmtkEventType type = smtk_event_get_event_type(event);
		SmtkEventState state = smtk_event_get_event_state(event);
		const char *key_name = smtk_event_get_key_name(event);
		if (type == SMTK_EVENT_TYPE_KEYBOARD_KEY &&
		    state == SMTK_EVENT_STATE_PRESSED &&
		    (strcmp(key_name, "KEY_LEFTALT") == 0 ||
		     strcmp(key_name, "KEY_RIGHTALT") == 0)) {
			if (emitter->alt_pressed) {
				trigger_pause_idle_function(emitter);
				emitter->alt_pressed = false;
			} else {
				emitter->alt_pressed = true;
			}
		} else {
			if (emitter->alt_pressed)
				emitter->alt_pressed = false;
		}

		char *key = NULL;
		// Always get key with SmtkKeysMapper, it will update XKB state
		// to keep sync with actual keyboard.
		switch (emitter->mode) {
		case SMTK_KEY_MODE_COMPOSED:
			key = smtk_keys_mapper_get_composed(emitter->mapper,
							    event);
			break;
		case SMTK_KEY_MODE_RAW:
			key = smtk_keys_mapper_get_raw(emitter->mapper, event);
			break;
		case SMTK_KEY_MODE_COMPACT:
			key = smtk_keys_mapper_get_compact(emitter->mapper,
							   event);
			break;
		default:
			// Should never be here.
			g_warn_if_reached();
			break;
		}
		if (key != NULL) {
			if (state == SMTK_EVENT_STATE_PRESSED &&
			    ((emitter->show_mouse &&
			      type == SMTK_EVENT_TYPE_POINTER_BUTTON) ||
			     (emitter->show_keyboard &&
			      type == SMTK_EVENT_TYPE_KEYBOARD_KEY)))
				trigger_key_idle_function(emitter, key);
			g_free(key);
		}
		g_clear_object(&event);
		g_free(line);
	}
	return NULL;
}

static void smtk_keys_emitter_init(SmtkKeysEmitter *emitter)
{
	emitter->settings = NULL;
	emitter->mapper = NULL;
	emitter->cli = NULL;
	emitter->cli_out = NULL;
	emitter->poller = NULL;
	emitter->error = NULL;
	emitter->show_keyboard = true;
	emitter->show_mouse = true;
	emitter->alt_pressed = false;
}

static void smtk_keys_emitter_constructed(GObject *object)
{
	// Seems we can only get constructor properties here.
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	emitter->mapper = smtk_keys_mapper_new(&emitter->error);
	// `emitter->error` is already set, just return.
	if (emitter->mapper == NULL)
		goto out;

	emitter->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(emitter->settings, "mode", emitter, "mode",
			G_SETTINGS_BIND_GET);
	g_settings_bind(emitter->settings, "show-keyboard", emitter,
			"show-keyboard", G_SETTINGS_BIND_GET);
	g_settings_bind(emitter->settings, "show-mouse", emitter, "show-mouse",
			G_SETTINGS_BIND_GET);

out:
	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->constructed(object);
}

static void smtk_keys_emitter_dispose(GObject *object)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	g_clear_object(&emitter->settings);
	g_clear_object(&emitter->mapper);

	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->dispose(object);
}

// static void smtk_keys_emitter_finalize(GObject *object)
// {
// 	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

// 	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->finalize(object);
// }

static void smtk_keys_emitter_class_init(SmtkKeysEmitterClass *emitter_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(emitter_class);

	object_class->set_property = smtk_keys_emitter_set_property;
	object_class->get_property = smtk_keys_emitter_get_property;

	object_class->constructed = smtk_keys_emitter_constructed;

	object_class->dispose = smtk_keys_emitter_dispose;
	// object_class->finalize = smtk_keys_emitter_finalize;

	obj_signals[SIG_KEY] = g_signal_new("key", SMTK_TYPE_KEYS_EMITTER,
					    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					    g_cclosure_marshal_VOID__STRING,
					    G_TYPE_NONE, 1, G_TYPE_STRING);
	obj_signals[SIG_ERROR_CLI_EXIT] = g_signal_new(
		"error-cli-exit", SMTK_TYPE_KEYS_EMITTER, G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	obj_props[PROP_MODE] = g_param_spec_enum(
		"mode", "Mode", "Key Mode", SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_SHOW_KEYBOARD] = g_param_spec_boolean(
		"show-keyboard", "Show Keyboard", "Show Keyboard Key", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPS, obj_props);
}

SmtkKeysEmitter *smtk_keys_emitter_new(GError **error)
{
	SmtkKeysEmitter *emitter = g_object_new(SMTK_TYPE_KEYS_EMITTER, NULL);

	if (emitter->error != NULL) {
		g_propagate_error(error, emitter->error);
		g_object_unref(emitter);
		return NULL;
	}

	return emitter;
}

static bool is_group(const char *group_name)
{
	gid_t *groups;
	int ngroups;
	struct group *grp;
	gid_t gid;

	ngroups = getgroups(0, NULL);
	groups = g_malloc(ngroups * sizeof(*groups));
	if (getgroups(ngroups, groups) < 0) {
		g_free(groups);
		return false;
	}

	grp = getgrnam(group_name);
	if (!grp) {
		g_free(groups);
		return false;
	}
	gid = grp->gr_gid;

	for (int i = 0; i < ngroups; i++) {
		if (groups[i] == gid) {
			g_free(groups);
			return true;
		}
	}

	g_free(groups);
	return false;
}

// Those two functions are splitted from init and dispose functions,
// because we need to pass a reference to the async callback of GTask,
// and don't want a loop reference (e.g. a emitter reference is dropped
// only when callback is called, and callback is called only when cli stopped,
// but cli is stopped only when emitter is disposed, and emitter is disposed
// only when reference is dropped!). So we break it into different functions
// and let the caller stop the cli before dispose.
void smtk_keys_emitter_start_async(SmtkKeysEmitter *emitter, GError **error)
{
	g_debug("Calling smtk_keys_emitter_start_async().");
	g_return_if_fail(emitter != NULL);

	if (is_group("input"))
		emitter->cli = g_subprocess_new(
			G_SUBPROCESS_FLAGS_STDIN_PIPE |
				G_SUBPROCESS_FLAGS_STDOUT_PIPE |
				G_SUBPROCESS_FLAGS_STDERR_PIPE,
			error, PACKAGE_BINDIR "/showmethekey-cli", NULL);
	else
		emitter->cli = g_subprocess_new(
			G_SUBPROCESS_FLAGS_STDIN_PIPE |
				G_SUBPROCESS_FLAGS_STDOUT_PIPE |
				G_SUBPROCESS_FLAGS_STDERR_PIPE,
			error, PKEXEC_PATH, PACKAGE_BINDIR "/showmethekey-cli",
			NULL);
	// emitter->error is already set, just return.
	if (emitter->cli == NULL)
		return;
	// Actually I don't wait the subprocess to return, they work like
	// clients and daemons, why clients want to wait for daemons' exiting?
	// This is just spawn subprocess.
	// smtk_keys_emitter_cli_on_complete is called when GTask finished,
	// and this is async and might the SmtkKeysWin is already destroyed.
	// So we have to manually reference to emitter here.
	g_subprocess_wait_check_async(emitter->cli, NULL,
				      smtk_keys_emitter_cli_on_complete,
				      g_object_ref(emitter));
	emitter->cli_out = g_data_input_stream_new(
		g_subprocess_get_stdout_pipe(emitter->cli));

	emitter->polling = true;
	emitter->poller =
		g_thread_try_new("poller", poller_function, emitter, error);
	// emitter->error is already set, just return.
	if (emitter->poller == NULL)
		return;
}

void smtk_keys_emitter_stop_async(SmtkKeysEmitter *emitter)
{
	g_debug("Calling smtk_keys_emitter_stop_async().");
	g_return_if_fail(emitter != NULL);

	// Don't know why but I need to stop cli before poller.
	if (emitter->cli != NULL) {
		// Because we run subprocess with pkexec,
		// so we cannot force kill it,
		// we use stdin pipe to write a "stop\n",
		// and let it exit by itself.
		const char stop[] = "stop\n";
		GBytes *input = g_bytes_new(stop, sizeof(stop));
		g_subprocess_communicate(emitter->cli, input, NULL, NULL, NULL,
					 NULL);
		g_bytes_unref(input);
		// g_subprocess_force_exit(emitter->cli);
		// Just close it, I am not interested in its error.
		g_input_stream_close(G_INPUT_STREAM(emitter->cli_out), NULL,
				     NULL);
		emitter->cli_out = NULL;
		g_object_unref(emitter->cli);
		emitter->cli = NULL;
	}

	if (emitter->poller != NULL) {
		emitter->polling = false;
		// This will wait until thread exited.
		// It will call g_thread_unref() internal
		// so we don't need to do it.
		g_thread_join(emitter->poller);
		// g_thread_unref(emitter->poller);
		emitter->poller = NULL;
	}
}
