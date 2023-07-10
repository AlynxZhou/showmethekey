#ifndef __SMTK_KEYMAP_LIST_H__
#define __SMTK_KEYMAP_LIST_H__

#include <stdbool.h>
#include <glib-object.h>

G_BEGIN_DECLS

bool keymap_is_default(const char *string);

#define SMTK_TYPE_KEYMAP_ITEM smtk_keymap_item_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeymapItem, smtk_keymap_item, SMTK, KEYMAP_ITEM,
		     GObject)

SmtkKeymapItem *smtk_keymap_item_new(const char *layout, const char *variant);
const char *smtk_keymap_item_get_layout(SmtkKeymapItem *keymap_item);
void smtk_keymap_item_set_layout(SmtkKeymapItem *keymap_item,
				 const char *layout);
const char *smtk_keymap_item_get_variant(SmtkKeymapItem *keymap_item);
void smtk_keymap_item_set_variant(SmtkKeymapItem *keymap_item,
				  const char *variant);
const char *smtk_keymap_item_get_name(SmtkKeymapItem *keymap_item);

#define SMTK_TYPE_KEYMAP_LIST smtk_keymap_list_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeymapList, smtk_keymap_list, SMTK, KEYMAP_LIST,
		     GObject)

SmtkKeymapList *smtk_keymap_list_new(void);
void smtk_keymap_list_append(SmtkKeymapList *keymap_list, const char *layout,
			     const char *variant);
void smtk_keymap_list_sort(SmtkKeymapList *keymap_list);
int smtk_keymap_list_find(SmtkKeymapList *keymap_list, const char *name);

G_END_DECLS

#endif
