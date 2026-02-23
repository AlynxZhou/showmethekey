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

SmtkKeysEmitter *smtk_keys_emitter_new(GError **error);
void smtk_keys_emitter_start_async(SmtkKeysEmitter *this, GError **error);
void smtk_keys_emitter_stop_async(SmtkKeysEmitter *this);

G_END_DECLS

#endif
