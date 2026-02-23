#ifndef __SMTK_KEYS_AREA_H__
#define __SMTK_KEYS_AREA_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_AREA smtk_keys_area_get_type()
G_DECLARE_FINAL_TYPE(
	SmtkKeysArea,
	smtk_keys_area,
	SMTK,
	KEYS_AREA,
	GtkDrawingArea
)

// It looks like glib-mkenums cannot make enums
// if clang-format make this in one line. But why???
// clang-format off
typedef enum {
	SMTK_KEY_ALIGNMENT_END,
	SMTK_KEY_ALIGNMENT_CENTER
} SmtkKeyAlignment;
// clang-format on

GtkWidget *smtk_keys_area_new(void);
void smtk_keys_area_add_key(SmtkKeysArea *area, const char *key);

G_END_DECLS

#endif
