#ifndef __SMTK_EVENT_H__
#define __SMTK_EVENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum _SmtkEventType {
	SMTK_EVENT_TYPE_UNKNOWN,
	SMTK_EVENT_TYPE_KEYBOARD_KEY,
	SMTK_EVENT_TYPE_POINTER_BUTTON
} SmtkEventType;

typedef enum _SmtkEventState {
	SMTK_EVENT_STATE_UNKNOWN,
	SMTK_EVENT_STATE_RELEASED,
	SMTK_EVENT_STATE_PRESSED
} SmtkEventState;

typedef struct _SmtkEvent {
	SmtkEventType type;
	SmtkEventState state;
	char *key_name;
	unsigned int key_code;
	unsigned int time_stamp;
} SmtkEvent;

#define SMTK_TYPE_EVENT smtk_event_get_type()

GType smtk_event_get_type(void);
SmtkEvent *smtk_event_new(const char *source);
SmtkEvent *smtk_event_copy(SmtkEvent *this);
void smtk_event_free(SmtkEvent *this);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SmtkEvent, smtk_event_free)

G_END_DECLS

#endif
