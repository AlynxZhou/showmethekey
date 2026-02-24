#include <json-glib/json-glib.h>

#include "smtk-event.h"

G_DEFINE_BOXED_TYPE(SmtkEvent, smtk_event, smtk_event_copy, smtk_event_free)

SmtkEvent *smtk_event_new(const char *source)
{
	g_return_val_if_fail(source != NULL, NULL);
	g_return_val_if_fail(strlen(source) > 0, NULL);

	g_autoptr(GError) error = NULL;
	g_autoptr(JsonNode) json = json_from_string(source, &error);
	if (error != NULL) {
		g_warning("Failed to parse JSON: %s.", error->message);
		return NULL;
	}
	JsonObject *json_object = json_node_get_object(json);
	if (json_object == NULL)
		return NULL;

	SmtkEvent *this = g_malloc0(sizeof(*this));

	const char *event_name =
		json_object_get_string_member(json_object, "event_name");
	if (g_strcmp0(event_name, "POINTER_BUTTON") == 0)
		this->type = SMTK_EVENT_TYPE_POINTER_BUTTON;
	else
		this->type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	const char *event_state_name =
		json_object_get_string_member(json_object, "state_name");
	if (g_strcmp0(event_state_name, "PRESSED") == 0)
		this->state = SMTK_EVENT_STATE_PRESSED;
	else
		this->state = SMTK_EVENT_STATE_RELEASED;
	this->key_name = g_strdup(
		json_object_get_string_member(json_object, "key_name")
	);
	this->key_code = json_object_get_int_member(json_object, "key_code");
	this->time_stamp =
		json_object_get_int_member(json_object, "time_stamp");

	return this;
}

SmtkEvent *smtk_event_copy(SmtkEvent *this)
{
	g_return_val_if_fail(this != NULL, NULL);

	SmtkEvent *that = g_malloc0(sizeof(*that));

	that->type = this->type;
	that->state = this->state;
	that->key_name = g_strdup(this->key_name);
	that->key_code = this->key_code;
	that->time_stamp = this->time_stamp;

	return that;
}

void smtk_event_free(SmtkEvent *this)
{
	g_return_if_fail(this != NULL);

	g_clear_pointer(&this->key_name, g_free);
	g_clear_pointer(&this, g_free);
}
