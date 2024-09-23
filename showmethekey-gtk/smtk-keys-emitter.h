#ifndef __SMTK_KEYS_EMITTER__
#define __SMTK_KEYS_EMITTER__

#include <stdbool.h>
#include <glib-object.h>

#include "smtk-keys-mapper.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_EMITTER smtk_keys_emitter_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysEmitter, smtk_keys_emitter, SMTK, KEYS_EMITTER,
		     GObject)

SmtkKeysEmitter *smtk_keys_emitter_new(bool show_shift, bool show_keyboard,
				       bool show_mouse, SmtkKeyMode mode,
				       const char *layout, const char *variant,
				       GError **error);
void smtk_keys_emitter_start_async(SmtkKeysEmitter *emitter, GError **error);
void smtk_keys_emitter_stop_async(SmtkKeysEmitter *emitter);
void smtk_keys_emitter_set_mode(SmtkKeysEmitter *emitter, SmtkKeyMode mode);
void smtk_keys_emitter_set_show_shift(SmtkKeysEmitter *emitter,
				      bool show_shift);
void smtk_keys_emitter_set_show_keyboard(SmtkKeysEmitter *emitter,
					 bool show_keyboard);
void smtk_keys_emitter_set_show_mouse(SmtkKeysEmitter *emitter,
				      bool show_mouse);
void smtk_keys_emitter_set_layout(SmtkKeysEmitter *emitter, const char *layout);
void smtk_keys_emitter_set_variant(SmtkKeysEmitter *emitter,
				   const char *variant);

G_END_DECLS

#endif
