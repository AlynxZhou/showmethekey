#include <json-glib/json-glib.h>

#include "smtk.h"
#include "smtk-event.h"

struct _SmtkEvent {
	GObject parent_instance;
	gchar *source;
	SmtkEventType event_type;
	SmtkEventState event_state;
	gchar *device_name;
	gchar *key_name;
	guint32 key_code;
	guint32 time_stamp;
	GError *error;
};
G_DEFINE_TYPE(SmtkEvent, smtk_event, G_TYPE_OBJECT)

// Prevent clang-format from adding space between minus.
// clang-format off
G_DEFINE_QUARK(smtk-event-error-quark, smtk_event_error)
// clang-format on

enum { PROP_0, PROP_SOURCE, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static void smtk_event_set_property(GObject *object, guint property_id,
				    const GValue *value, GParamSpec *pspec)
{
	SmtkEvent *event = SMTK_EVENT(object);

	switch (property_id) {
	case PROP_SOURCE:
		if (event->source != NULL)
			g_free(event->source);
		event->source = g_value_dup_string(value);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_event_get_property(GObject *object, guint property_id,
				    GValue *value, GParamSpec *pspec)
{
	SmtkEvent *event = SMTK_EVENT(object);

	switch (property_id) {
	case PROP_SOURCE:
		g_value_set_string(value, event->source);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_event_init(SmtkEvent *event)
{
	event->error = NULL;
	event->source = NULL;
	event->device_name = NULL;
	event->key_name = NULL;
	event->event_type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	event->event_state = SMTK_EVENT_STATE_RELEASED;
}

static void smtk_event_constructed(GObject *object)
{
	SmtkEvent *event = SMTK_EVENT(object);

	if (event->source == NULL || strlen(event->source) == 0) {
		g_set_error(&event->error, SMTK_EVENT_ERROR, SMTK_EVENT_ERROR_SOURCE, "Failed to create event because of empty source.");
		return;
	}
	// See <https://developer.gnome.org/json-glib/stable/json-glib-Utility-API.html#json-from-string>.
	// Transfer full, we should free it.
	JsonNode *json = json_from_string(event->source, &event->error);
	if (json == NULL)
		return;
	// See <https://developer.gnome.org/json-glib/stable/json-glib-JSON-Node.html#json-node-get-object>.
	// Transfer none, so we can't free this.
	JsonObject *json_object = json_node_get_object(json);
	// See <https://developer.gnome.org/json-glib/stable/json-glib-JSON-Object.html#json-object-get-member>.
	// Transfer none, don't free it.
	const char *event_name =
		json_object_get_string_member(json_object, "event_name");
	if (g_strcmp0(event_name, "POINTER_BUTTON") == 0)
		event->event_type = SMTK_EVENT_TYPE_POINTER_BUTTON;
	else
		event->event_type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	const char *event_state_name =
		json_object_get_string_member(json_object, "state_name");
	if (g_strcmp0(event_state_name, "PRESSED") == 0)
		event->event_state = SMTK_EVENT_STATE_PRESSED;
	else
		event->event_state = SMTK_EVENT_STATE_RELEASED;
	// See <https://developer.gnome.org/json-glib/stable/json-glib-JSON-Object.html#json-object-get-member>.
	// Transfer none, so we need g_strdup().
	event->device_name =
		g_strdup(json_object_get_string_member(json_object, "device_name"));
	event->key_name =
		g_strdup(json_object_get_string_member(json_object, "key_name"));
	event->key_code = json_object_get_int_member(json_object, "key_code");
	event->time_stamp = json_object_get_int_member(json_object, "time_stamp");
	json_node_unref(json);
}

static void smtk_event_finalize(GObject *object)
{
	SmtkEvent *event = SMTK_EVENT(object);
	if (event->source != NULL) {
		g_free(event->source);
		event->source = NULL;
	}
	if (event->device_name != NULL) {
		g_free(event->device_name);
		event->device_name = NULL;
	}
	if (event->key_name != NULL) {
		g_free(event->key_name);
		event->key_name = NULL;
	}
}

static void smtk_event_class_init(SmtkEventClass *event_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(event_class);

	object_class->set_property = smtk_event_set_property;
	object_class->get_property = smtk_event_get_property;

	object_class->constructed = smtk_event_constructed;

	object_class->finalize = smtk_event_finalize;

	obj_properties[PROP_SOURCE] =
		g_param_spec_string("source", "Source", "Event Text Source",
				    NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

SmtkEvent *smtk_event_new(gchar *source, GError **error)
{
	SmtkEvent *event = g_object_new(SMTK_TYPE_EVENT, "source", source, NULL);

	if (event->error != NULL) {
		g_propagate_error(error, event->error);
		g_object_unref(event);
		return NULL;
	}

	return event;
}

SmtkEventType smtk_event_get_event_type(SmtkEvent *event)
{
	return event->event_type;
}

SmtkEventState smtk_event_get_event_state(SmtkEvent *event)
{
	return event->event_state;
}

gchar *smtk_event_get_device_name(SmtkEvent *event)
{
	return g_strdup(event->device_name);
}

gchar *smtk_event_get_key_name(SmtkEvent *event)
{
	return g_strdup(event->key_name);
}

guint32 smtk_event_get_key_code(SmtkEvent *event)
{
	return event->key_code;
}

guint32 smtk_event_get_time_stamp(SmtkEvent *event)
{
	return event->time_stamp;
}
