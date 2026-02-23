#include <json-glib/json-glib.h>

#include "smtk-event.h"

// TODO: Turn this into a boxed type?
struct _SmtkEvent {
	GObject parent_instance;
	char *source;
	SmtkEventType event_type;
	SmtkEventState event_state;
	char *key_name;
	unsigned int key_code;
	unsigned int time_stamp;
	GError *error;
};
G_DEFINE_TYPE(SmtkEvent, smtk_event, G_TYPE_OBJECT)

// Prevent clang-format from adding space between minus.
// clang-format off
G_DEFINE_QUARK(smtk-event-error-quark, smtk_event_error)
// clang-format on

enum { PROP_0, PROP_SOURCE, N_PROPS };

static GParamSpec *props[N_PROPS] = { NULL };

static void set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkEvent *this = SMTK_EVENT(o);

	switch (prop) {
	case PROP_SOURCE:
		g_clear_pointer(&this->source, g_free);
		this->source = g_value_dup_string(value);
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
	SmtkEvent *this = SMTK_EVENT(o);

	switch (prop) {
	case PROP_SOURCE:
		g_value_set_string(value, this->source);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void constructed(GObject *o)
{
	SmtkEvent *this = SMTK_EVENT(o);

	if (this->source == NULL || strlen(this->source) == 0) {
		g_set_error(
			&this->error,
			SMTK_EVENT_ERROR,
			SMTK_EVENT_ERROR_SOURCE,
			"Failed to create event because of empty source."
		);
		return;
	}
	// See <https://developer.gnome.org/json-glib/stable/json-glib-Utility-API.html#json-from-string>.
	// Transfer full, we should free it.
	g_autoptr(JsonNode) json = json_from_string(this->source, &this->error);
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
		this->event_type = SMTK_EVENT_TYPE_POINTER_BUTTON;
	else
		this->event_type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	const char *event_state_name =
		json_object_get_string_member(json_object, "state_name");
	if (g_strcmp0(event_state_name, "PRESSED") == 0)
		this->event_state = SMTK_EVENT_STATE_PRESSED;
	else
		this->event_state = SMTK_EVENT_STATE_RELEASED;
	// See <https://developer.gnome.org/json-glib/stable/json-glib-JSON-Object.html#json-object-get-member>.
	// Transfer none, so we need g_strdup().
	this->key_name = g_strdup(
		json_object_get_string_member(json_object, "key_name")
	);
	this->key_code = json_object_get_int_member(json_object, "key_code");
	this->time_stamp =
		json_object_get_int_member(json_object, "time_stamp");

	G_OBJECT_CLASS(smtk_event_parent_class)->constructed(o);
}

static void finalize(GObject *o)
{
	SmtkEvent *this = SMTK_EVENT(o);

	g_clear_pointer(&this->source, g_free);
	g_clear_pointer(&this->key_name, g_free);

	G_OBJECT_CLASS(smtk_event_parent_class)->finalize(o);
}

static void smtk_event_class_init(SmtkEventClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);

	o_class->set_property = set_property;
	o_class->get_property = get_property;

	o_class->constructed = constructed;

	o_class->finalize = finalize;

	props[PROP_SOURCE] = g_param_spec_string(
		"source",
		"Source",
		"Event Text Source",
		NULL,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_event_init(SmtkEvent *this)
{
	this->error = NULL;
	this->source = NULL;
	this->key_name = NULL;
	this->event_type = SMTK_EVENT_TYPE_KEYBOARD_KEY;
	this->event_state = SMTK_EVENT_STATE_RELEASED;
}

SmtkEvent *smtk_event_new(char *source, GError **error)
{
	SmtkEvent *this = g_object_new(SMTK_TYPE_EVENT, "source", source, NULL);

	if (this->error != NULL) {
		g_propagate_error(error, this->error);
		g_object_unref(this);
		return NULL;
	}

	return this;
}

SmtkEventType smtk_event_get_event_type(SmtkEvent *this)
{
	g_return_val_if_fail(this != NULL, SMTK_EVENT_TYPE_UNKNOWN);

	return this->event_type;
}

SmtkEventState smtk_event_get_event_state(SmtkEvent *this)
{
	g_return_val_if_fail(this != NULL, SMTK_EVENT_STATE_UNKNOWN);

	return this->event_state;
}

const char *smtk_event_get_key_name(SmtkEvent *this)
{
	g_return_val_if_fail(this != NULL, NULL);

	return this->key_name;
}

unsigned int smtk_event_get_key_code(SmtkEvent *this)
{
	// 0 is KEY_RESERVED for evdev.
	g_return_val_if_fail(this != NULL, 0);

	return this->key_code;
}

unsigned int smtk_event_get_time_stamp(SmtkEvent *this)
{
	g_return_val_if_fail(this != NULL, 0);

	return this->time_stamp;
}
