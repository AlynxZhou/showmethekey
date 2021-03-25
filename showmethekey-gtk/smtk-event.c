#include <json-glib/json-glib.h>

#include "smtk.h"
#include "smtk-event.h"

struct _SmtkEvent {
	GObject parent_instance;
	SmtkEventType event_type;
	SmtkEventState event_state;
	gchar *device_name;
	gchar *key_name;
	guint32 key_code;
	guint32 time_stamp;
};
G_DEFINE_TYPE(SmtkEvent, smtk_event, G_TYPE_OBJECT)

// TODO: Maybe stop using GObject for event?
// Or add properties to make it a full GObject?

static void smtk_event_init(SmtkEvent *event)
{
	event->device_name = NULL;
	event->key_name = NULL;
}

static void smtk_event_finalize(GObject *object)
{
	SmtkEvent *event = SMTK_EVENT(object);
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
	G_OBJECT_CLASS(event_class)->finalize = smtk_event_finalize;
}

SmtkEvent *smtk_event_new(void)
{
	return g_object_new(SMTK_TYPE_EVENT, NULL);
}

int smtk_event_parse_json(SmtkEvent *event, const char *json_line)
{
	if (json_line == NULL)
		return -1;
	GError *json_error = NULL;
	JsonNode *json = json_from_string(json_line, &json_error);
	if (json == NULL) {
		g_critical("Parse JSON failed: %s.\n", json_error->message);
		g_error_free(json_error);
		return -2;
	}
	JsonObject *object = json_node_get_object(json);
	const char *event_name =
		json_object_get_string_member(object, "event_name");
	if (g_strcmp0(event_name, "POINTER_BUTTON") == 0)
		event->event_type = SMTK_EVENT_TYPE_POINTER_BUTTON;
	else
		event->event_type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	const char *event_state_name =
		json_object_get_string_member(object, "state_name");
	if (g_strcmp0(event_state_name, "PRESSED") == 0)
		event->event_state = SMTK_EVENT_STATE_PRESSED;
	else
		event->event_state = SMTK_EVENT_STATE_RELEASED;
	event->device_name =
		g_strdup(json_object_get_string_member(object, "device_name"));
	event->key_name =
		g_strdup(json_object_get_string_member(object, "key_name"));
	event->key_code = json_object_get_int_member(object, "key_code");
	event->time_stamp = json_object_get_int_member(object, "time_stamp");
	return 0;
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
