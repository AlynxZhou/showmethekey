#ifndef __SMTK_KEYS_MAPPER_H__
#define __SMTK_KEYS_MAPPER_H__

#include <stdbool.h>
#include <glib-object.h>

#include "smtk-event.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_MAPPER smtk_keys_mapper_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysMapper, smtk_keys_mapper, SMTK, KEYS_MAPPER, GObject)

#define SMTK_KEYS_MAPPER_ERROR smtk_keys_mapper_error_quark()
typedef enum {
	SMTK_KEYS_MAPPER_ERROR_XKB_CONTEXT,
	SMTK_KEYS_MAPPER_ERROR_XKB_KEYMAP,
	SMTK_KEYS_MAPPER_ERROR_XKB_STATE,
	SMTK_KEYS_MAPPER_ERROR_UNKNOWN
} SmtkKeysMapperError;

// It looks like glib-mkenums cannot make enums
// if clang-format make this in one line. But why???
// clang-format off
typedef enum {
	SMTK_KEY_MODE_COMPOSED,
	SMTK_KEY_MODE_RAW,
	SMTK_KEY_MODE_COMPACT
} SmtkKeyMode;
// clang-format on

SmtkKeysMapper *smtk_keys_mapper_new(GError **error);
char *smtk_keys_mapper_get_raw(SmtkKeysMapper *this, SmtkEvent *event);
char *smtk_keys_mapper_get_composed(SmtkKeysMapper *this, SmtkEvent *event);
char *smtk_keys_mapper_get_compact(SmtkKeysMapper *this, SmtkEvent *event);

G_END_DECLS

#endif
