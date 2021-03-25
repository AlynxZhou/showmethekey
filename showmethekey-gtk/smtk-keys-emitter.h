#ifndef __SMTK_KEYS_EMITTER__
#define __SMTK_KEYS_EMITTER__

#include <glib-object.h>

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_EMITTER smtk_keys_emitter_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysEmitter, smtk_keys_emitter, SMTK, KEYS_EMITTER, GObject)

typedef enum {
	SMTK_KEY_MODE_COMPOSED,
	SMTK_KEY_MODE_RAW
} SmtkKeyMode;

SmtkKeysEmitter *smtk_keys_emitter_new(SmtkKeyMode mode);

G_END_DECLS

#endif
