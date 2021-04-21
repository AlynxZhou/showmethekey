#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "smtk.h"
#include "smtk-keys-emitter.h"
#include "smtk-keys-mapper.h"
#include "smtk-event.h"

#define MAX_KEYS 30

struct _SmtkKeysEmitter {
	GObject parent_instance;
	SmtkKeysMapper *mapper;
	GSubprocess *cli;
	GDataInputStream *cli_out;
	GThread *poller;
	gboolean polling;
	GPtrArray *keys_array;
	SmtkKeyMode mode;
	gboolean show_mouse;
	gboolean waiting;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysEmitter, smtk_keys_emitter, G_TYPE_OBJECT)

enum { SIG_UPDATE_LABEL, SIG_ERROR_CLI_EXIT, N_SIGNALS };

static guint obj_signals[N_SIGNALS] = { 0 };

enum { PROP_0, PROP_MODE, PROP_SHOW_MOUSE, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

// Check whether user choose cancel for pkexec.
static void smtk_keys_emitter_cli_on_complete(GObject *source_object,
					      GAsyncResult *res,
					      gpointer user_data)
{
	// We got a copy of SmtkKeysEmitter's address when setting up this
	// callback, and there is a condition, that user closes the switch,
	// and emitter is disposed, then this callback is called, and the
	// address we hold is invalid, it might point to other objects, and
	// still not NULL. To solve this problem we hold a reference to this
	// callback to prevent the emitter to be disposed and manually drop it.
	g_debug("smtk_keys_emitter_cli_on_complete() called.");

	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(user_data);
	// Cli may already released normally, and this function only cares about
	// when cli exited with error, for example user cancelled pkexec.
	if (emitter->cli != NULL &&
	    g_subprocess_get_exit_status(emitter->cli) != 0) {
		// Better to close thread here to prevent a lot of error.
		emitter->polling = FALSE;
		if (emitter->poller != NULL) {
			g_debug("Stop poller because cli exitted.");
			g_thread_join(emitter->poller);
			emitter->poller = NULL;
		}
		g_signal_emit_by_name(emitter, "error-cli-exit");
	}
	g_object_unref(emitter);
}

static void smtk_keys_emitter_set_property(GObject *object, guint property_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	switch (property_id) {
	case PROP_MODE:
		emitter->mode = g_value_get_enum(value);
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

static void smtk_keys_emitter_get_property(GObject *object, guint property_id,
					   GValue *value, GParamSpec *pspec)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	switch (property_id) {
	case PROP_MODE:
		g_value_set_enum(value, emitter->mode);
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

static void idle_destroy_function(gpointer user_data)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(user_data);
	g_object_unref(emitter);
}

static gboolean idle_function(gpointer user_data)
{
	// Here we back to UI thread.
	// Looks like we may have many idle_function() run at the same time.
	// So we cannot use a common label_text in emitter.
	// Instead we only join them here.
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(user_data);
	// Need to use sans, monospace cannot be smaller.
	gchar *label_text = g_strjoinv(
		"<span font_family=\"sans\" size=\"smaller\"> </span>",
		(gchar **)emitter->keys_array->pdata);
	g_signal_emit_by_name(emitter, "update-label", label_text);
	g_free(label_text);
	return FALSE;
}

static gpointer poller_function(gpointer user_data)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(user_data);
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

		// Don't handle if skip mouse button.
		if (!emitter->show_mouse &&
		    smtk_event_get_event_type(event) ==
			    SMTK_EVENT_TYPE_POINTER_BUTTON) {
			g_object_unref(event);
			g_free(line);
			continue;
		}

		gchar *key;
		// Always get key with SmtkKeysMapper, it will update XKB state.
		switch (emitter->mode) {
		case SMTK_KEY_MODE_COMPOSED:
			key = smtk_keys_mapper_get_composed(emitter->mapper,
							    event);
			break;
		case SMTK_KEY_MODE_RAW:
			key = smtk_keys_mapper_get_raw(emitter->mapper, event);
			break;
		default:
			// Should never be here.
			g_warn_if_reached();
			break;
		}
		if (key != NULL) {
			// Don't save key if hiding.
			if (smtk_event_get_event_state(event) ==
				    SMTK_EVENT_STATE_PRESSED &&
			    !emitter->waiting) {
				// We don't free inserted text here,
				// GPtrArray will free them.
				gchar *escaped = g_markup_escape_text(key, -1);
				gchar *marked = g_strconcat("<u>", escaped,
							    "</u>", NULL);
				g_free(escaped);
				g_ptr_array_insert(emitter->keys_array,
						   emitter->keys_array->len - 1,
						   marked);
				if (emitter->keys_array->len - 1 > MAX_KEYS)
					// We set free function for GPtrArray,
					// So it will free automatically.
					g_ptr_array_remove_range(
						emitter->keys_array, 0,
						emitter->keys_array->len - 1 -
							MAX_KEYS);
				// UI can only be modified in UI thread,
				// and we are not in UI thread here.
				// So we need to use `g_timeout_add()` to kick
				// an async callback into glib's main loop
				// (the same as GTK UI thread).
				// Signals are not async!
				// So we cannot emit signal here,
				// because they will run in poller thread
				// instead of UI thread.
				// `g_idle_add()` is not suitable because we
				// have a high priority.
				g_timeout_add_full(G_PRIORITY_DEFAULT, 0,
						   idle_function,
						   g_object_ref(emitter),
						   idle_destroy_function);
			}
			g_free(key);
		}
		g_object_unref(event);
		g_free(line);
	}
	return NULL;
}

static void smtk_keys_emitter_init(SmtkKeysEmitter *emitter)
{
	emitter->mapper = NULL;
	emitter->cli = NULL;
	emitter->cli_out = NULL;
	emitter->poller = NULL;
	emitter->keys_array = NULL;
	emitter->error = NULL;

	emitter->mapper = smtk_keys_mapper_new(&emitter->error);
	// emitter->error is already set, just return.
	if (emitter->mapper == NULL)
		return;
	// g_strjoinv() accepts a NULL terminated char pointer array,
	// so we use a GPtrArray to store char pointer.
	emitter->keys_array = g_ptr_array_new_full(MAX_KEYS + 1, g_free);
	// Append a NULL first and always insert elements before it.
	// So we can directly use the GPtrArray for g_strjoinv().
	g_ptr_array_add(emitter->keys_array, NULL);
}

static void smtk_keys_emitter_finalize(GObject *object)
{
	SmtkKeysEmitter *emitter = SMTK_KEYS_EMITTER(object);

	if (emitter->keys_array != NULL) {
		g_ptr_array_free(emitter->keys_array, TRUE);
		emitter->keys_array = NULL;
	}

	G_OBJECT_CLASS(smtk_keys_emitter_parent_class)->finalize(object);
}

static void smtk_keys_emitter_class_init(SmtkKeysEmitterClass *emitter_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(emitter_class);

	object_class->set_property = smtk_keys_emitter_set_property;
	object_class->get_property = smtk_keys_emitter_get_property;

	object_class->finalize = smtk_keys_emitter_finalize;

	obj_signals[SIG_UPDATE_LABEL] = g_signal_new(
		"update-label", SMTK_TYPE_KEYS_EMITTER, G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
		G_TYPE_STRING);
	obj_signals[SIG_ERROR_CLI_EXIT] = g_signal_new(
		"error-cli-exit", SMTK_TYPE_KEYS_EMITTER, G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	obj_properties[PROP_MODE] =
		g_param_spec_enum("mode", "Mode", "Key Mode",
				  SMTK_TYPE_KEY_MODE, SMTK_KEY_MODE_COMPOSED,
				  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	obj_properties[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", TRUE,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

SmtkKeysEmitter *smtk_keys_emitter_new(gboolean show_mouse, SmtkKeyMode mode,
				       GError **error)
{
	SmtkKeysEmitter *emitter = g_object_new(SMTK_TYPE_KEYS_EMITTER, "mode",
						mode, "show-mouse", show_mouse,
						NULL);

	if (emitter->error != NULL) {
		g_propagate_error(error, emitter->error);
		g_object_unref(emitter);
		return NULL;
	}

	return emitter;
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
	g_debug("smtk_keys_emitter_start_async() called.");
	g_return_if_fail(emitter != NULL);

	emitter->cli = g_subprocess_new(
		G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
			G_SUBPROCESS_FLAGS_STDERR_PIPE,
		error, "pkexec", INSTALL_PREFIX "/bin/showmethekey-cli", NULL);
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

	emitter->polling = TRUE;
	emitter->poller =
		g_thread_try_new("poller", poller_function, emitter, error);
	// emitter->error is already set, just return.
	if (emitter->poller == NULL)
		return;
}

void smtk_keys_emitter_stop_async(SmtkKeysEmitter *emitter)
{
	g_debug("smtk_keys_emitter_stop_async() called.");
	g_return_if_fail(emitter != NULL);

	// Don't know why but I need to stop cli before poller.
	if (emitter->cli != NULL) {
		// Because we run subprocess with pkexec,
		// so we cannot force kill it,
		// we use stdin pipe to write a "stop\n",
		// and let it exit by itself.
		const char *stop = "stop\n";
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
		emitter->polling = FALSE;
		// This will wait until thread exited.
		// It will call g_thread_unref() internal
		// so we don't need to do it.
		g_thread_join(emitter->poller);
		// g_thread_unref(emitter->poller);
		emitter->poller = NULL;
	}

	if (emitter->mapper != NULL) {
		g_object_unref(emitter->mapper);
		emitter->mapper = NULL;
	}
}

void smtk_keys_emitter_pause(SmtkKeysEmitter *emitter)
{
	g_return_if_fail(emitter != NULL);

	emitter->waiting = TRUE;
}

void smtk_keys_emitter_resume(SmtkKeysEmitter *emitter)
{
	g_return_if_fail(emitter != NULL);

	emitter->waiting = FALSE;
}
