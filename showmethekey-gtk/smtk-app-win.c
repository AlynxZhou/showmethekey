#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>
#include <xkbcommon/xkbregistry.h>

#include "config.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-usage-win.h"
#include "smtk-keys-area.h"
#include "smtk-keys-mapper.h"
#include "smtk-keymap-list.h"

struct _SmtkAppWin {
	AdwApplicationWindow parent_instance;
	struct rxkb_context *rxkb_context;
	GSettings *settings;
	GtkWidget *menu_button;
	GtkWidget *keys_win_switch;
	GtkWidget *clickable_switch;
	GtkWidget *pause_switch;
	GtkWidget *shift_switch;
	GtkWidget *keyboard_switch;
	GtkWidget *mouse_switch;
	GtkWidget *border_switch;
	GtkWidget *hide_visible_switch;
	GtkWidget *mode_selector;
	GtkWidget *alignment_selector;
	GtkWidget *width_entry;
	GtkWidget *height_entry;
	GtkWidget *margin_slider;
	GtkWidget *timeout_entry;
	GtkWidget *keymap_selector;
};
G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, ADW_TYPE_APPLICATION_WINDOW)

static int mode_to_selector(GValue *value, GVariant *variant, void *data)
{
	const char *mode = NULL;
	g_variant_get(variant, "&s", &mode);

	if (g_strcmp0(mode, "composed") == 0)
		g_value_set_uint(value, SMTK_KEY_MODE_COMPOSED);
	else if (g_strcmp0(mode, "raw") == 0)
		g_value_set_uint(value, SMTK_KEY_MODE_RAW);
	else if (g_strcmp0(mode, "compact") == 0)
		g_value_set_uint(value, SMTK_KEY_MODE_COMPACT);
	else
		g_value_set_uint(value, SMTK_KEY_MODE_COMPOSED);

	return true;
}

static GVariant *selector_to_mode(
	const GValue *value,
	const GVariantType *expected_type,
	void *data
)
{
	switch (g_value_get_uint(value)) {
	case SMTK_KEY_MODE_COMPOSED:
		return g_variant_new_string("composed");
	case SMTK_KEY_MODE_RAW:
		return g_variant_new_string("raw");
	case SMTK_KEY_MODE_COMPACT:
		return g_variant_new_string("compact");
	default:
		return g_variant_new_string("composed");
	}
}

static int alignment_to_selector(GValue *value, GVariant *variant, void *data)
{
	const char *alignment = NULL;
	g_variant_get(variant, "&s", &alignment);

	if (g_strcmp0(alignment, "end") == 0)
		g_value_set_uint(value, SMTK_KEY_ALIGNMENT_END);
	else if (g_strcmp0(alignment, "center") == 0)
		g_value_set_uint(value, SMTK_KEY_ALIGNMENT_CENTER);
	else
		g_value_set_uint(value, SMTK_KEY_ALIGNMENT_END);

	return true;
}

static GVariant *selector_to_alignment(
	const GValue *value,
	const GVariantType *expected_type,
	void *data
)
{
	switch (g_value_get_uint(value)) {
	case SMTK_KEY_ALIGNMENT_END:
		return g_variant_new_string("end");
	case SMTK_KEY_ALIGNMENT_CENTER:
		return g_variant_new_string("center");
	default:
		return g_variant_new_string("end");
	}
}

static void
on_selected(SmtkAppWin *this, GParamSpec *prop, AdwComboRow *keymap_selector)
{
	GObject *item = adw_combo_row_get_selected_item(
		ADW_COMBO_ROW(this->keymap_selector)
	);
	g_autofree char *layout = NULL;
	g_autofree char *variant = NULL;

	g_object_get(item, "layout", &layout, "variant", &variant, NULL);
	g_settings_set_string(this->settings, "layout", layout);
	g_settings_set_string(this->settings, "variant", variant);
}

static void constructed(GObject *o)
{
	SmtkAppWin *this = SMTK_APP_WIN(o);

	// If we want to bind ui template file to class, we must put menu into
	// another file.
	g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource(
		"/one/alynx/showmethekey/smtk-app-win-menu.ui"
	);
	GMenuModel *menu_model =
		G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
	gtk_menu_button_set_menu_model(
		GTK_MENU_BUTTON(this->menu_button), menu_model
	);

	gtk_window_set_focus(GTK_WINDOW(this), NULL);
	gtk_window_set_default_widget(GTK_WINDOW(this), this->keys_win_switch);

	g_autoptr(SmtkKeymapList) klist = smtk_keymap_list_new();
	g_autoptr(GtkExpression) name_expression = gtk_property_expression_new(
		SMTK_TYPE_KEYMAP_ITEM, NULL, "name"
	);
	adw_combo_row_set_expression(
		ADW_COMBO_ROW(this->keymap_selector), name_expression
	);
	adw_combo_row_set_model(
		ADW_COMBO_ROW(this->keymap_selector), G_LIST_MODEL(klist)
	);
	this->rxkb_context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
	if (this->rxkb_context != NULL &&
	    rxkb_context_parse_default_ruleset(this->rxkb_context)) {
		for (struct rxkb_layout *rxkb_layout =
			     rxkb_layout_first(this->rxkb_context);
		     rxkb_layout != NULL;
		     rxkb_layout = rxkb_layout_next(rxkb_layout))
			smtk_keymap_list_append(
				klist,
				rxkb_layout_get_name(rxkb_layout),
				rxkb_layout_get_variant(rxkb_layout)
			);
		smtk_keymap_list_sort(klist);
	}

	this->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(
		this->settings,
		"active",
		this->keys_win_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"clickable",
		this->clickable_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"paused",
		this->pause_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"show-shift",
		this->shift_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"show-keyboard",
		this->keyboard_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"show-mouse",
		this->mouse_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"draw-border",
		this->border_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"hide-visible",
		this->hide_visible_switch,
		"active",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind_with_mapping(
		this->settings,
		"mode",
		this->mode_selector,
		"selected",
		G_SETTINGS_BIND_DEFAULT,
		mode_to_selector,
		selector_to_mode,
		this,
		NULL
	);
	g_settings_bind_with_mapping(
		this->settings,
		"alignment",
		this->alignment_selector,
		"selected",
		G_SETTINGS_BIND_DEFAULT,
		alignment_to_selector,
		selector_to_alignment,
		this,
		NULL
	);
	g_settings_bind(
		this->settings,
		"width",
		this->width_entry,
		"value",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"height",
		this->height_entry,
		"value",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"margin-ratio",
		gtk_range_get_adjustment(GTK_RANGE(this->margin_slider)),
		"value",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"timeout",
		this->timeout_entry,
		"value",
		G_SETTINGS_BIND_DEFAULT
	);
	// See <https://docs.gtk.org/gio/method.Settings.bind_with_mapping.html>.
	//
	// We are unable to bind settings with keymap selector, because we have
	// 2 different keys for keymap, but each property can only be bound once.
	// It is OK to only load them on constructed, and save them on changed.
	g_autofree char *layout =
		g_settings_get_string(this->settings, "layout");
	g_autofree char *variant =
		g_settings_get_string(this->settings, "variant");
	GListModel *keymaps =
		adw_combo_row_get_model(ADW_COMBO_ROW(this->keymap_selector));
	int position = smtk_keymap_list_find(
		SMTK_KEYMAP_LIST(keymaps), layout, variant
	);
	adw_combo_row_set_selected(
		ADW_COMBO_ROW(this->keymap_selector),
		position > 0 ? position : 0
	);
	g_signal_connect_swapped(
		this->keymap_selector,
		"notify::selected",
		G_CALLBACK(on_selected),
		this
	);
	if (g_settings_get_boolean(this->settings, "first-time")) {
		smtk_app_win_show_usage(this);
		g_settings_set_boolean(this->settings, "first-time", false);
	}

	G_OBJECT_CLASS(smtk_app_win_parent_class)->constructed(o);
}

static void dispose(GObject *o)
{
	SmtkAppWin *this = SMTK_APP_WIN(o);

	g_clear_object(&this->settings);

	g_clear_pointer(&this->rxkb_context, rxkb_context_unref);

	G_OBJECT_CLASS(smtk_app_win_parent_class)->dispose(o);
}

static void smtk_app_win_class_init(SmtkAppWinClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *w_class = GTK_WIDGET_CLASS(klass);

	o_class->constructed = constructed;
	o_class->dispose = dispose;

	gtk_widget_class_set_template_from_resource(
		w_class, "/one/alynx/showmethekey/smtk-app-win.ui"
	);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, keys_win_switch
	);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, menu_button);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, clickable_switch
	);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, pause_switch);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, shift_switch);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, keyboard_switch
	);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, mouse_switch);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, border_switch);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, hide_visible_switch
	);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, mode_selector);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, alignment_selector
	);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, width_entry);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, height_entry);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, margin_slider);
	gtk_widget_class_bind_template_child(w_class, SmtkAppWin, timeout_entry);
	gtk_widget_class_bind_template_child(
		w_class, SmtkAppWin, keymap_selector
	);
}

static void smtk_app_win_init(SmtkAppWin *this)
{
	gtk_widget_init_template(GTK_WIDGET(this));
	this->settings = NULL;
}

GtkWidget *smtk_app_win_new(SmtkApp *app)
{
	return g_object_new(SMTK_TYPE_APP_WIN, "application", app, NULL);
}

void smtk_app_win_show_usage(SmtkAppWin *this)
{
	g_return_if_fail(this != NULL);

	GtkWidget *usage_win = smtk_usage_win_new();
	gtk_window_present(GTK_WINDOW(usage_win));
}

void smtk_app_win_show_about(SmtkAppWin *this)
{
	g_return_if_fail(this != NULL);

	const char *developers[] = { "Alynx Zhou",    "LGiki",	      "mimir-d",
				     "Jakub Jirutka", "Eli Schwartz", "Ariel",
				     "LordRishav",    "Bobby Rong",   NULL };

	const char *artists[] = { "Freepik", NULL };

	const char *documenters[] = { "Alynx Zhou", "Pedro Sade Azevedo",
				      "Mridhul",    "WhiredPlanck",
				      "Jan Beich",  NULL };

	const char license[] =
		"Licensed under the Apache License, "
		"Version 2.0 (the \"License\");\n"
		"you may not use this file except in "
		"compliance with the License.\n"
		"You may obtain a copy of the License at\n\n"

		"    http://www.apache.org/licenses/LICENSE-2.0\n\n"

		"Unless required by applicable law or "
		"agreed to in writing, software\n"
		"distributed under the License is distributed "
		"on an \"AS IS\" BASIS,\n"
		"WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, "
		"either express or implied.\n"
		"See the License for the specific language "
		"governing permissions and\n"
		"limitations under the License.";

#if ADW_CHECK_VERSION(1, 5, 0)
	adw_show_about_dialog(
		GTK_WIDGET(this),
		"developers",
		developers,
#else
	adw_show_about_window(
		GTK_WINDOW(this),
		"developers",
		developers,
#endif
		"artists",
		artists,
		"documenters",
		documenters,
		"translator-credits",
		_("translator-credits"),
		"title",
		_("About Show Me The Key"),
		"application-name",
		_("Show Me The Key"),
		"comments",
		_("Show keys you typed on screen."),
		"copyright",
		"Copyright © 2021-2022 Alynx Zhou",
		"license",
		license,
		"application-icon",
		"one.alynx.showmethekey",
		"website",
		"https://showmethekey.alynx.one/",
		"version",
		PROJECT_VERSION,
		NULL
	);
}
