#ifndef __SMTK_KEYS_AREA_H__
#define __SMTK_KEYS_AREA_H__

#include <gtk/gtk.h>

#include "smtk-keys-emitter.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_AREA smtk_keys_area_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysArea, smtk_keys_area, SMTK, KEYS_AREA,
		     GtkDrawingArea)

GtkWidget *smtk_keys_area_new(bool draw_border, int timeout);
void smtk_keys_area_set_draw_border(SmtkKeysArea *area, bool draw_border);
void smtk_keys_area_set_timeout(SmtkKeysArea *area, int timeout);
void smtk_keys_area_add_key(SmtkKeysArea *area, char *key);

G_END_DECLS

#endif
