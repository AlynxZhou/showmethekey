#include <gio/gio.h>

#include "smtk-keymap-list.h"

bool smtk_keymap_is_default(const char *string)
{
	return string == NULL || strlen(string) == 0 ||
	       g_strcmp0(string, "(null)") == 0 ||
	       g_strcmp0(string, "default") == 0;
}

struct _SmtkKeymapItem {
	GObject parent_instance;
	char *layout;
	char *variant;
	char *name;
};
G_DEFINE_TYPE(SmtkKeymapItem, smtk_keymap_item, G_TYPE_OBJECT)

enum { PROP_0, PROP_LAYOUT, PROP_VARIANT, PROP_NAME, N_PROPS };

static GParamSpec *obj_props[N_PROPS] = { NULL };

static char *build_name(const char *layout, const char *variant)
{
	if (smtk_keymap_is_default(variant))
		return g_strdup(layout);
	return g_strdup_printf("%s (%s)", layout, variant);
}

static void smtk_keymap_item_set_property(GObject *object,
					  unsigned int property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	SmtkKeymapItem *keymap_item = SMTK_KEYMAP_ITEM(object);

	switch (property_id) {
	case PROP_LAYOUT:
		smtk_keymap_item_set_layout(keymap_item,
					    g_value_get_string(value));
		break;
	case PROP_VARIANT:
		smtk_keymap_item_set_variant(keymap_item,
					     g_value_get_string(value));
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keymap_item_get_property(GObject *object,
					  unsigned int property_id,
					  GValue *value, GParamSpec *pspec)
{
	SmtkKeymapItem *keymap_item = SMTK_KEYMAP_ITEM(object);

	switch (property_id) {
	case PROP_LAYOUT:
		g_value_set_string(value, keymap_item->layout);
		break;
	case PROP_VARIANT:
		g_value_set_string(value, keymap_item->variant);
		break;
	case PROP_NAME:
		g_value_set_string(value, keymap_item->name);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keymap_item_finalize(GObject *object)
{
	SmtkKeymapItem *keymap_item = SMTK_KEYMAP_ITEM(object);

	g_clear_pointer(&keymap_item->layout, g_free);
	g_clear_pointer(&keymap_item->variant, g_free);
	g_clear_pointer(&keymap_item->name, g_free);
}

static void smtk_keymap_item_init(SmtkKeymapItem *keymap_item)
{
}

static void smtk_keymap_item_class_init(SmtkKeymapItemClass *keymap_item_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(keymap_item_class);

	object_class->set_property = smtk_keymap_item_set_property;
	object_class->get_property = smtk_keymap_item_get_property;

	object_class->finalize = smtk_keymap_item_finalize;

	obj_props[PROP_LAYOUT] =
		g_param_spec_string("layout", "Layout", "Keymap Layout", NULL,
				    G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_VARIANT] = g_param_spec_string(
		"variant", "Variant", "Keymap Variant", NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_NAME] = g_param_spec_string(
		"name", "Name", "Keymap Display Name", NULL, G_PARAM_READABLE);

	g_object_class_install_properties(object_class, N_PROPS, obj_props);
}

SmtkKeymapItem *smtk_keymap_item_new(const char *layout, const char *variant)
{
	return g_object_new(SMTK_TYPE_KEYMAP_ITEM, "layout", layout, "variant",
			    variant, NULL);
}

const char *smtk_keymap_item_get_layout(SmtkKeymapItem *keymap_item)
{
	g_return_val_if_fail(keymap_item != NULL, NULL);

	return keymap_item->layout;
}

void smtk_keymap_item_set_layout(SmtkKeymapItem *keymap_item,
				 const char *layout)
{
	g_return_if_fail(keymap_item != NULL);

	g_clear_pointer(&keymap_item->layout, g_free);
	keymap_item->layout = g_strdup(layout);

	g_clear_pointer(&keymap_item->name, g_free);
	keymap_item->name =
		build_name(keymap_item->layout, keymap_item->variant);
}

const char *smtk_keymap_item_get_variant(SmtkKeymapItem *keymap_item)
{
	g_return_val_if_fail(keymap_item != NULL, NULL);

	return keymap_item->variant;
}

void smtk_keymap_item_set_variant(SmtkKeymapItem *keymap_item,
				  const char *variant)
{
	g_return_if_fail(keymap_item != NULL);

	g_clear_pointer(&keymap_item->variant, g_free);
	keymap_item->variant = g_strdup(variant);

	g_clear_pointer(&keymap_item->name, g_free);
	keymap_item->name =
		build_name(keymap_item->layout, keymap_item->variant);
}

const char *smtk_keymap_item_get_name(SmtkKeymapItem *keymap_item)
{
	g_return_val_if_fail(keymap_item != NULL, NULL);

	return keymap_item->name;
}

struct _SmtkKeymapList {
	GObject parent_instance;
	GPtrArray *items;
};

static GType smtk_keymap_list_get_item_type(GListModel *list)
{
	return G_TYPE_OBJECT;
}

static unsigned int smtk_keymap_list_get_n_items(GListModel *list)
{
	SmtkKeymapList *keymap_list = SMTK_KEYMAP_LIST(list);

	return keymap_list->items->len;
}

static void *smtk_keymap_list_get_item(GListModel *list, unsigned int position)
{
	SmtkKeymapList *keymap_list = SMTK_KEYMAP_LIST(list);

	if (position >= keymap_list->items->len)
		return NULL;

	return g_object_ref(keymap_list->items->pdata[position]);
}

static void smtk_keymap_list_model_init(GListModelInterface *iface)
{
	iface->get_item_type = smtk_keymap_list_get_item_type;
	iface->get_n_items = smtk_keymap_list_get_n_items;
	iface->get_item = smtk_keymap_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE(SmtkKeymapList, smtk_keymap_list, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL,
					      smtk_keymap_list_model_init))

static void smtk_keymap_list_finalize(GObject *object)
{
	SmtkKeymapList *keymap_list = SMTK_KEYMAP_LIST(object);

	g_clear_pointer(&keymap_list->items, g_ptr_array_unref);
}

static void smtk_keymap_list_init(SmtkKeymapList *keymap_list)
{
	keymap_list->items = g_ptr_array_new_with_free_func(g_object_unref);
}

static void smtk_keymap_list_class_init(SmtkKeymapListClass *keymap_list_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(keymap_list_class);

	object_class->finalize = smtk_keymap_list_finalize;
}

SmtkKeymapList *smtk_keymap_list_new(void)
{
	return g_object_new(SMTK_TYPE_KEYMAP_LIST, NULL);
}

void smtk_keymap_list_append(SmtkKeymapList *keymap_list, const char *layout,
			     const char *variant)
{
	g_return_if_fail(keymap_list != NULL);

	g_ptr_array_add(keymap_list->items,
			smtk_keymap_item_new(layout, variant));

	g_list_model_items_changed(G_LIST_MODEL(keymap_list),
				   keymap_list->items->len - 1, 0, 1);
}

static int _compare(gconstpointer a, gconstpointer b)
{
#if GLIB_CHECK_VERSION(2, 76, 0)
	const char *name1 =
		smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM((void *)a));
	const char *name2 =
		smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM((void *)b));
#else
	const char *name1 =
		smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM(*(void **)a));
	const char *name2 =
		smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM(*(void **)b));
#endif
	// Most people use US (QWERTY) layout, so make it first to be the
	// default value.
	if (g_strcmp0(name1, "us") == 0)
		return -1;
	if (g_strcmp0(name2, "us") == 0)
		return 1;
	return g_strcmp0(name1, name2);
}

void smtk_keymap_list_sort(SmtkKeymapList *keymap_list)
{
	g_return_if_fail(keymap_list != NULL);
#if GLIB_CHECK_VERSION(2, 76, 0)
	g_ptr_array_sort_values(keymap_list->items, _compare);
#else
	g_ptr_array_sort(keymap_list->items, _compare);
#endif
	g_list_model_items_changed(G_LIST_MODEL(keymap_list), 0,
				   keymap_list->items->len,
				   keymap_list->items->len);
}

static int _equal(gconstpointer a, gconstpointer b)
{
	const char *name1 =
		smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM((void *)a));
	const char *name2 = b;

	return g_strcmp0(name1, name2) == 0;
}

int smtk_keymap_list_find(SmtkKeymapList *keymap_list, const char *layout,
			  const char *variant)
{
	g_return_val_if_fail(keymap_list != NULL, -1);

	g_autofree char *name = build_name(layout, variant);
	unsigned int position;

	if (g_ptr_array_find_with_equal_func(keymap_list->items, name, _equal,
					     &position))
		return position;

	return -1;
}
