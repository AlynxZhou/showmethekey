#ifndef __SMTK_KEYMAP_LIST_H__
#define __SMTK_KEYMAP_LIST_H__

#include <stdbool.h>
#include <glib-object.h>

G_BEGIN_DECLS

bool smtk_keymap_is_default(const char *string);

#define SMTK_TYPE_KEYMAP_ITEM smtk_keymap_item_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeymapItem, smtk_keymap_item, SMTK, KEYMAP_ITEM, GObject)

#define SMTK_TYPE_KEYMAP_LIST smtk_keymap_list_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeymapList, smtk_keymap_list, SMTK, KEYMAP_LIST, GObject)

SmtkKeymapList *smtk_keymap_list_new(void);
void smtk_keymap_list_append(
	SmtkKeymapList *this,
	const char *layout,
	const char *variant
);
void smtk_keymap_list_sort(SmtkKeymapList *this);
int smtk_keymap_list_find(
	SmtkKeymapList *this,
	const char *layout,
	const char *variant
);

G_END_DECLS

#endif
