#ifndef __SMTK_KEYS_EMITTER__
#define __SMTK_KEYS_EMITTER__

#include <stdbool.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_EMITTER smtk_keys_emitter_get_type()
G_DECLARE_FINAL_TYPE(
	SmtkKeysEmitter,
	smtk_keys_emitter,
	SMTK,
	KEYS_EMITTER,
	GObject
)

// It looks like glib-mkenums cannot make enums
// if clang-format make this in one line. But why???
// clang-format off
typedef enum _SmtkModifier {
	SMTK_MODIFIER_NONE,
	SMTK_MODIFIER_CTRL,
	SMTK_MODIFIER_ALT,
	SMTK_MODIFIER_SUPER,
	SMTK_MODIFIER_SHIFT,
	SMTK_MODIFIER_ESC
} SmtkModifier;
// clang-format on

SmtkKeysEmitter *smtk_keys_emitter_new(void);
void smtk_keys_emitter_start_async(SmtkKeysEmitter *this);
void smtk_keys_emitter_stop_async(SmtkKeysEmitter *this);

G_END_DECLS

#endif
