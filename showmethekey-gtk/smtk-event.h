#ifndef __SMTK_EVENT_H__
#define __SMTK_EVENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	SMTK_EVENT_TYPE_UNKNOWN,
	SMTK_EVENT_TYPE_KEYBOARD_KEY,
	SMTK_EVENT_TYPE_POINTER_BUTTON
} SmtkEventType;

typedef enum {
	SMTK_EVENT_STATE_UNKNOWN,
	SMTK_EVENT_STATE_RELEASED,
	SMTK_EVENT_STATE_PRESSED
} SmtkEventState;

#define SMTK_EVENT_ERROR smtk_event_error_quark()
typedef enum {
	SMTK_EVENT_ERROR_SOURCE,
	SMTK_EVENT_ERROR_XKB_UNKNOWN
} SmtkEventError;

#define SMTK_TYPE_EVENT smtk_event_get_type()
G_DECLARE_FINAL_TYPE(SmtkEvent, smtk_event, SMTK, EVENT, GObject)

SmtkEvent *smtk_event_new(char *source, GError **error);
SmtkEventType smtk_event_get_event_type(SmtkEvent *event);
SmtkEventState smtk_event_get_event_state(SmtkEvent *event);
const char *smtk_event_get_device_name(SmtkEvent *event);
const char *smtk_event_get_key_name(SmtkEvent *event);
unsigned int smtk_event_get_key_code(SmtkEvent *event);
unsigned int smtk_event_get_time_stamp(SmtkEvent *event);

G_END_DECLS

#endif
