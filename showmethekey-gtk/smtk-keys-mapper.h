#ifndef __SMTK_KEYS_MAPPER_H__
#define __SMTK_KEYS_MAPPER_H__

#include <glib-object.h>

#include "smtk-event.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_MAPPER smtk_keys_mapper_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysMapper, smtk_keys_mapper, SMTK, KEYS_MAPPER, GObject)

SmtkKeysMapper* smtk_keys_mapper_new(void);
char *smtk_keys_mapper_get_raw(SmtkKeysMapper *mapper, SmtkEvent *event);
char *smtk_keys_mapper_get_composed(SmtkKeysMapper *mapper, SmtkEvent *event);

G_END_DECLS

#endif
