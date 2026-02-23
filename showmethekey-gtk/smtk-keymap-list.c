#include <gio/gio.h>

#include "smtk-keymap-list.h"

bool smtk_keymap_is_default(const char *string)
{
	return string == NULL || strlen(string) == 0 ||
	       g_strcmp0(string, "(null)") == 0 ||
	       g_strcmp0(string, "default") == 0;
}

// NOTE: This could not be a boxed type because GtkListBoxRow requires GObject.
struct _SmtkKeymapItem {
	GObject parent_instance;
	char *layout;
	char *variant;
	char *name;
};
G_DEFINE_TYPE(SmtkKeymapItem, smtk_keymap_item, G_TYPE_OBJECT)

enum { PROP_0, PROP_LAYOUT, PROP_VARIANT, PROP_NAME, N_PROPS };

static GParamSpec *props[N_PROPS] = { NULL };

static char *build_name(const char *layout, const char *variant)
{
	if (smtk_keymap_is_default(variant))
		return g_strdup(layout);
	return g_strdup_printf("%s (%s)", layout, variant);
}

static void item_set_layout(SmtkKeymapItem *this, const char *layout)
{
	g_clear_pointer(&this->layout, g_free);
	this->layout = g_strdup(layout);

	g_clear_pointer(&this->name, g_free);
	this->name = build_name(this->layout, this->variant);
}

static void item_set_variant(SmtkKeymapItem *this, const char *variant)
{
	g_clear_pointer(&this->variant, g_free);
	// Use empty string so we don't need to check NULL on reading item.
	this->variant = variant != NULL ? g_strdup(variant) : g_strdup("");

	g_clear_pointer(&this->name, g_free);
	this->name = build_name(this->layout, this->variant);
}

static void item_set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeymapItem *this = SMTK_KEYMAP_ITEM(o);

	switch (prop) {
	case PROP_LAYOUT:
		item_set_layout(this, g_value_get_string(value));
		break;
	case PROP_VARIANT:
		item_set_variant(this, g_value_get_string(value));
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void item_get_property(
	GObject *o,
	unsigned int prop,
	GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeymapItem *this = SMTK_KEYMAP_ITEM(o);

	switch (prop) {
	case PROP_LAYOUT:
		g_value_set_string(value, this->layout);
		break;
	case PROP_VARIANT:
		g_value_set_string(value, this->variant);
		break;
	case PROP_NAME:
		g_value_set_string(value, this->name);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void item_finalize(GObject *o)
{
	SmtkKeymapItem *this = SMTK_KEYMAP_ITEM(o);

	g_clear_pointer(&this->layout, g_free);
	g_clear_pointer(&this->variant, g_free);
	g_clear_pointer(&this->name, g_free);

	G_OBJECT_CLASS(smtk_keymap_item_parent_class)->finalize(o);
}

static void smtk_keymap_item_class_init(SmtkKeymapItemClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);

	o_class->set_property = item_set_property;
	o_class->get_property = item_get_property;

	o_class->finalize = item_finalize;

	props[PROP_LAYOUT] = g_param_spec_string(
		"layout",
		"Layout",
		"Keymap Layout",
		NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_VARIANT] = g_param_spec_string(
		"variant",
		"Variant",
		"Keymap Variant",
		NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_NAME] = g_param_spec_string(
		"name", "Name", "Keymap Display Name", NULL, G_PARAM_READABLE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_keymap_item_init(SmtkKeymapItem *this)
{
}

static SmtkKeymapItem *item_new(const char *layout, const char *variant)
{
	return g_object_new(
		SMTK_TYPE_KEYMAP_ITEM, "layout", layout, "variant", variant, NULL
	);
}

struct _SmtkKeymapList {
	GObject parent_instance;
	GPtrArray *items;
};

static GType list_get_item_type(GListModel *list)
{
	return G_TYPE_OBJECT;
}

static unsigned int list_get_n_items(GListModel *list)
{
	SmtkKeymapList *this = SMTK_KEYMAP_LIST(list);

	return this->items->len;
}

static void *list_get_item(GListModel *list, unsigned int position)
{
	SmtkKeymapList *this = SMTK_KEYMAP_LIST(list);

	if (position >= this->items->len)
		return NULL;

	return g_object_ref(this->items->pdata[position]);
}

static void list_model_init(GListModelInterface *iface)
{
	iface->get_item_type = list_get_item_type;
	iface->get_n_items = list_get_n_items;
	iface->get_item = list_get_item;
}

G_DEFINE_TYPE_WITH_CODE(
	SmtkKeymapList,
	smtk_keymap_list,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, list_model_init)
)

static void list_finalize(GObject *o)
{
	SmtkKeymapList *this = SMTK_KEYMAP_LIST(o);

	g_clear_pointer(&this->items, g_ptr_array_unref);

	G_OBJECT_CLASS(smtk_keymap_list_parent_class)->finalize(o);
}

static void smtk_keymap_list_class_init(SmtkKeymapListClass *keymap_list_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(keymap_list_class);

	object_class->finalize = list_finalize;
}

static void smtk_keymap_list_init(SmtkKeymapList *this)
{
	this->items = g_ptr_array_new_with_free_func(g_object_unref);
}

SmtkKeymapList *smtk_keymap_list_new(void)
{
	return g_object_new(SMTK_TYPE_KEYMAP_LIST, NULL);
}

void smtk_keymap_list_append(
	SmtkKeymapList *this,
	const char *layout,
	const char *variant
)
{
	g_return_if_fail(this != NULL);
	g_return_if_fail(layout != NULL);

	g_ptr_array_add(this->items, item_new(layout, variant));

	g_list_model_items_changed(
		G_LIST_MODEL(this), this->items->len - 1, 0, 1
	);
}

static int compare(gconstpointer a, gconstpointer b)
{
#if GLIB_CHECK_VERSION(2, 76, 0)
	const char *name1 = SMTK_KEYMAP_ITEM((void *)a)->name;
	const char *name2 = SMTK_KEYMAP_ITEM((void *)b)->name;
#else
	const char *name1 = SMTK_KEYMAP_ITEM(*(void **)a)->name;
	const char *name2 = SMTK_KEYMAP_ITEM(*(void **)b)->name;
#endif
	// Most people use US (QWERTY) layout, so make it first to be the
	// default value.
	if (g_strcmp0(name1, "us") == 0)
		return -1;
	if (g_strcmp0(name2, "us") == 0)
		return 1;
	return g_strcmp0(name1, name2);
}

void smtk_keymap_list_sort(SmtkKeymapList *this)
{
	g_return_if_fail(this != NULL);
#if GLIB_CHECK_VERSION(2, 76, 0)
	g_ptr_array_sort_values(this->items, compare);
#else
	g_ptr_array_sort(this->items, compare);
#endif
	g_list_model_items_changed(
		G_LIST_MODEL(this), 0, this->items->len, this->items->len
	);
}

static int equal(gconstpointer a, gconstpointer b)
{
	const char *name1 = SMTK_KEYMAP_ITEM((void *)a)->name;
	const char *name2 = b;

	return g_strcmp0(name1, name2) == 0;
}

int smtk_keymap_list_find(
	SmtkKeymapList *this,
	const char *layout,
	const char *variant
)
{
	g_return_val_if_fail(this != NULL, -1);

	g_autofree char *name = build_name(layout, variant);
	unsigned int position;

	if (g_ptr_array_find_with_equal_func(
		    this->items, name, equal, &position
	    ))
		return position;

	return -1;
}
