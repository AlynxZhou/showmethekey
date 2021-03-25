#ifndef __SMTK_EVENT_H__
#define __SMTK_EVENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	SMTK_EVENT_TYPE_KEYBOARD_KEY,
	SMTK_EVENT_TYPE_POINTER_BUTTON
} SmtkEventType;

typedef enum {
	SMTK_EVENT_STATE_RELEASED,
	SMTK_EVENT_STATE_PRESSED
} SmtkEventState;

#define SMTK_TYPE_EVENT smtk_event_get_type()
G_DECLARE_FINAL_TYPE(SmtkEvent, smtk_event, SMTK, EVENT, GObject)

SmtkEvent *smtk_event_new(void);
int smtk_event_parse_json(SmtkEvent *event, const char *json_line);
SmtkEventType smtk_event_get_event_type(SmtkEvent *event);
SmtkEventState smtk_event_get_event_state(SmtkEvent *event);
gchar *smtk_event_get_device_name(SmtkEvent *event);
gchar *smtk_event_get_key_name(SmtkEvent *event);
guint32 smtk_event_get_key_code(SmtkEvent *event);
guint32 smtk_event_get_time_stamp(SmtkEvent *event);

G_END_DECLS

#endif
