#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>
#include <xkbcommon/xkbregistry.h>

#include "smtk.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"
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
	GtkWidget *mode_selector;
	GtkWidget *width_entry;
	GtkWidget *height_entry;
	GtkWidget *timeout_entry;
	GtkWidget *keymap_selector;
	GtkWidget *keys_win;
};
G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, ADW_TYPE_APPLICATION_WINDOW)

static SmtkKeyMode smtk_app_win_get_mode(SmtkAppWin *win)
{
	int mode_id =
		adw_combo_row_get_selected(ADW_COMBO_ROW(win->mode_selector));
	g_debug("Mode: %d.", mode_id);
	switch (mode_id) {
	case 0:
		return SMTK_KEY_MODE_COMPOSED;
	case 1:
		return SMTK_KEY_MODE_RAW;
	case 2:
		return SMTK_KEY_MODE_COMPACT;
	default:
		return SMTK_KEY_MODE_COMPOSED;
	}
}

static void smtk_app_win_get_keymap(SmtkAppWin *win, const char **layout,
				    const char **variant)
{
	GObject *keymap = adw_combo_row_get_selected_item(
		ADW_COMBO_ROW(win->keymap_selector));
	*layout = smtk_keymap_item_get_layout(SMTK_KEYMAP_ITEM(keymap));
	*variant = smtk_keymap_item_get_variant(SMTK_KEYMAP_ITEM(keymap));
	g_debug("Keymap: %s (%s).", *layout, *variant);
}

static void smtk_app_win_enable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->clickable_switch, false);
	gtk_widget_set_sensitive(win->pause_switch, false);
	gtk_widget_set_sensitive(win->width_entry, true);
	gtk_widget_set_sensitive(win->height_entry, true);
}

static void smtk_app_win_disable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->clickable_switch, true);
	gtk_widget_set_sensitive(win->pause_switch, true);
	gtk_widget_set_sensitive(win->width_entry, false);
	gtk_widget_set_sensitive(win->height_entry, false);
}

static void smtk_app_win_keys_win_on_destroy(SmtkAppWin *win,
					     gpointer user_data,
					     SmtkKeysWin *keys_win)
{
	if (win->keys_win != NULL) {
		// Should set this first as we check it in callback.
		win->keys_win = NULL;

		gtk_switch_set_active(GTK_SWITCH(win->keys_win_switch), false);
		// Clickable by default.
		gtk_switch_set_active(GTK_SWITCH(win->clickable_switch), true);
		gtk_switch_set_active(GTK_SWITCH(win->pause_switch), false);
		smtk_app_win_enable(win);
	}
}

void smtk_app_win_activate(SmtkAppWin *win)
{
	const bool show_shift =
		gtk_switch_get_active(GTK_SWITCH(win->shift_switch));
	const bool show_keyboard =
		gtk_switch_get_active(GTK_SWITCH(win->keyboard_switch));
	const bool show_mouse =
		gtk_switch_get_active(GTK_SWITCH(win->mouse_switch));
	const bool draw_border =
		gtk_switch_get_active(GTK_SWITCH(win->border_switch));

	SmtkKeyMode mode = smtk_app_win_get_mode(win);

	const int timeout = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(win->timeout_entry));
	g_debug("Timeout: %d.", timeout);

	int width = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(win->width_entry));
	width = width <= 0 ? 1500 : width;
	int height = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(win->height_entry));
	height = height <= 0 ? 200 : height;
	g_debug("Size: %d×%d.", width, height);

	const char *layout = NULL;
	const char *variant = NULL;
	smtk_app_win_get_keymap(win, &layout, &variant);

	GError *error = NULL;
	win->keys_win = smtk_keys_win_new(win, show_shift, show_keyboard,
					  show_mouse, draw_border, mode, width,
					  height, timeout, layout, variant,
					  &error);
	if (win->keys_win == NULL) {
		g_warning("%s", error->message);
		g_error_free(error);
		gtk_switch_set_active(GTK_SWITCH(win->keys_win_switch), false);
		return;
	}
	g_signal_connect_object(win->keys_win, "pause",
				G_CALLBACK(smtk_app_win_toggle_pause_switch),
				win, G_CONNECT_SWAPPED);
	g_signal_connect_object(win->keys_win, "destroy",
				G_CALLBACK(smtk_app_win_keys_win_on_destroy),
				win, G_CONNECT_SWAPPED);

	gtk_window_present(GTK_WINDOW(win->keys_win));

	gtk_switch_set_active(GTK_SWITCH(win->keys_win_switch), true);
	smtk_app_win_disable(win);
}

// See <https://mail.gnome.org/archives/networkmanager-list/2010-October/msg00129.html>.
// notify of property have one more argument for property
// in the middle of instance and object.
static void smtk_app_win_on_keys_win_switch_active(SmtkAppWin *win,
						   GParamSpec *prop,
						   GtkSwitch *keys_win_switch)
{
	if (gtk_switch_get_active(GTK_SWITCH(win->keys_win_switch))) {
		if (win->keys_win == NULL)
			smtk_app_win_activate(win);
	} else {
		// We clear pointer and change widget states in signal handler.
		if (win->keys_win != NULL)
			gtk_window_destroy(GTK_WINDOW(win->keys_win));
	}
}

static void smtk_app_win_on_clickable_switch_active(SmtkAppWin *win,
						    GParamSpec *prop,
						    GtkSwitch *clickable_switch)
{
	// This only works when keys_win is open.
	// Calling `gtk_switch_set_active()` also triggers this, but then we
	// don't have a `keys_win` at that time.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_clickable(
		SMTK_KEYS_WIN(win->keys_win),
		gtk_switch_get_active(GTK_SWITCH(win->clickable_switch)));
}

static void smtk_app_win_on_pause_switch_active(SmtkAppWin *win,
						GParamSpec *prop,
						GtkSwitch *pause_switch)
{
	// This only works when keys_win is open.
	// We don't disable or turn off it here,
	// doing this when keys_win destroyed is enough.
	if (win->keys_win == NULL)
		return;

	if (gtk_switch_get_active(GTK_SWITCH(win->pause_switch)))
		smtk_keys_win_pause(SMTK_KEYS_WIN(win->keys_win));
	else
		smtk_keys_win_resume(SMTK_KEYS_WIN(win->keys_win));
}

static void smtk_app_win_on_shift_switch_active(SmtkAppWin *win,
						GParamSpec *prop,
						GtkSwitch *shift_switch)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_show_shift(
		SMTK_KEYS_WIN(win->keys_win),
		gtk_switch_get_active(GTK_SWITCH(win->shift_switch)));
}

static void smtk_app_win_on_keyboard_switch_active(SmtkAppWin *win,
						   GParamSpec *prop,
						   GtkSwitch *keyboard_switch)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_show_keyboard(
		SMTK_KEYS_WIN(win->keys_win),
		gtk_switch_get_active(GTK_SWITCH(win->keyboard_switch)));
}

static void smtk_app_win_on_mouse_switch_active(SmtkAppWin *win,
						GParamSpec *prop,
						GtkSwitch *mouse_switch)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_show_mouse(
		SMTK_KEYS_WIN(win->keys_win),
		gtk_switch_get_active(GTK_SWITCH(win->mouse_switch)));
}

static void smtk_app_win_on_border_switch_active(SmtkAppWin *win,
						 GParamSpec *prop,
						 GtkSwitch *mouse_switch)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_draw_border(
		SMTK_KEYS_WIN(win->keys_win),
		gtk_switch_get_active(GTK_SWITCH(win->border_switch)));
}

static void smtk_app_win_on_mode_selector_selected(SmtkAppWin *win,
						   GParamSpec *prop,
						   AdwComboRow *mode_selector)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	smtk_keys_win_set_mode(SMTK_KEYS_WIN(win->keys_win),
			       smtk_app_win_get_mode(win));
}

static void smtk_app_win_on_timeout_value(SmtkAppWin *win, GParamSpec *prop,
					  GtkSpinButton *timeout_entry)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	int timeout = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(win->timeout_entry));
	g_debug("Timeout: %d.", timeout);

	smtk_keys_win_set_timeout(SMTK_KEYS_WIN(win->keys_win), timeout);
}

static void
smtk_app_win_on_keymap_selector_selected(SmtkAppWin *win, GParamSpec *prop,
					 AdwComboRow *keymap_selector)
{
	// This only works when keys_win is open.
	if (win->keys_win == NULL)
		return;

	const char *layout = NULL;
	const char *variant = NULL;
	smtk_app_win_get_keymap(win, &layout, &variant);

	smtk_keys_win_set_layout(SMTK_KEYS_WIN(win->keys_win), layout);
	smtk_keys_win_set_variant(SMTK_KEYS_WIN(win->keys_win), variant);
}

static int settings_to_object(GValue *value, GVariant *variant,
			      gpointer user_data)
{
	SmtkAppWin *win = user_data;
	char *name = NULL;

	g_variant_get(variant, "&s", &name);

	if (name == NULL || strlen(name) == 0)
		return -1;

	GListModel *keymap_list =
		adw_combo_row_get_model(ADW_COMBO_ROW(win->keymap_selector));
	int position =
		smtk_keymap_list_find(SMTK_KEYMAP_LIST(keymap_list), name);
	if (position < 0)
		return -1;

	g_value_set_uint(value, position);
	return 1;
}

static GVariant *object_to_settings(const GValue *value,
				    const GVariantType *expected_type,
				    gpointer user_data)
{
	SmtkAppWin *win = user_data;
	unsigned int position = g_value_get_uint(value);
	GListModel *keymap_list =
		adw_combo_row_get_model(ADW_COMBO_ROW(win->keymap_selector));
	GObject *keymap = g_list_model_get_object(keymap_list, position);
	const char *name = smtk_keymap_item_get_name(SMTK_KEYMAP_ITEM(keymap));

	if (name == NULL || strlen(name) == 0)
		return NULL;

	char *format_string = g_variant_type_dup_string(expected_type);
	GVariant *variant = g_variant_new(format_string, name);
	g_free(format_string);
	return variant;
}

static void smtk_app_win_init(SmtkAppWin *win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
	// If we want to bind ui template file to class, we must put menu into
	// another file.
	GtkBuilder *builder = gtk_builder_new_from_resource(
		"/one/alynx/showmethekey/smtk-app-win-menu.ui");
	GMenuModel *menu_model =
		G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(win->menu_button),
				       menu_model);
	g_object_unref(builder);

	gtk_window_set_focus(GTK_WINDOW(win), NULL);
	gtk_window_set_default_widget(GTK_WINDOW(win), win->keys_win_switch);

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->timeout_entry), 0,
				  30000);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->timeout_entry), 100,
				       1000);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->width_entry), 0,
				  INT_MAX);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->width_entry), 100,
				       500);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->height_entry), 0,
				  INT_MAX);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->height_entry), 100,
				       500);

	SmtkKeymapList *keymap_list = smtk_keymap_list_new();
	GtkExpression *name_expression = gtk_property_expression_new(
		SMTK_TYPE_KEYMAP_ITEM, NULL, "name");
	adw_combo_row_set_expression(ADW_COMBO_ROW(win->keymap_selector),
				     name_expression);
	adw_combo_row_set_model(ADW_COMBO_ROW(win->keymap_selector),
				G_LIST_MODEL(keymap_list));
	win->rxkb_context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
	if (win->rxkb_context != NULL &&
	    rxkb_context_parse_default_ruleset(win->rxkb_context)) {
		for (struct rxkb_layout *rxkb_layout =
			     rxkb_layout_first(win->rxkb_context);
		     rxkb_layout != NULL;
		     rxkb_layout = rxkb_layout_next(rxkb_layout))
			smtk_keymap_list_append(
				keymap_list, rxkb_layout_get_name(rxkb_layout),
				rxkb_layout_get_variant(rxkb_layout));
		smtk_keymap_list_sort(keymap_list);
	}

	win->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(win->settings, "show-shift", win->shift_switch,
			"active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "show-keyboard", win->keyboard_switch,
			"active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "show-mouse", win->mouse_switch,
			"active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "draw-border", win->border_switch,
			"active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "mode", win->mode_selector, "selected",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "width", win->width_entry, "value",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "height", win->height_entry, "value",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "timeout", win->timeout_entry, "value",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind_with_mapping(win->settings, "keymap",
				     win->keymap_selector, "selected",
				     G_SETTINGS_BIND_DEFAULT,
				     settings_to_object, object_to_settings,
				     win, NULL);

	if (g_settings_get_boolean(win->settings, "first-time")) {
		smtk_app_win_show_usage_dialog(win);
		g_settings_set_boolean(win->settings, "first-time", false);
	}
}

static void smtk_app_win_dispose(GObject *object)
{
	SmtkAppWin *win = SMTK_APP_WIN(object);

	g_clear_object(&win->settings);

	g_clear_pointer(&win->rxkb_context, rxkb_context_unref);

	// Manually destroy keys_win, so CLI backend will be told to stop.、
	if (win->keys_win != NULL) {
		gtk_window_destroy(GTK_WINDOW(win->keys_win));
		win->keys_win = NULL;
	}

	G_OBJECT_CLASS(smtk_app_win_parent_class)->dispose(object);
}

static void smtk_app_win_class_init(SmtkAppWinClass *win_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(win_class);

	object_class->dispose = smtk_app_win_dispose;

	gtk_widget_class_set_template_from_resource(
		GTK_WIDGET_CLASS(win_class),
		"/one/alynx/showmethekey/smtk-app-win.ui");
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, keys_win_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, menu_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, clickable_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, pause_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, shift_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, keyboard_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, mouse_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, border_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, mode_selector);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, width_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, height_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, timeout_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, keymap_selector);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_keys_win_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_clickable_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_pause_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_shift_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_keyboard_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_mouse_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_border_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_mode_selector_selected);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(win_class),
						smtk_app_win_on_timeout_value);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_keymap_selector_selected);
}

GtkWidget *smtk_app_win_new(SmtkApp *app)
{
	return g_object_new(SMTK_TYPE_APP_WIN, "application", app, NULL);
}

void smtk_app_win_toggle_clickable_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->clickable_switch))
		gtk_switch_set_active(GTK_SWITCH(win->clickable_switch),
				      !gtk_switch_get_active(GTK_SWITCH(
					      win->clickable_switch)));
}

void smtk_app_win_toggle_pause_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->pause_switch))
		gtk_switch_set_active(
			GTK_SWITCH(win->pause_switch),
			!gtk_switch_get_active(GTK_SWITCH(win->pause_switch)));
}

void smtk_app_win_toggle_shift_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->shift_switch))
		gtk_switch_set_active(
			GTK_SWITCH(win->shift_switch),
			!gtk_switch_get_active(GTK_SWITCH(win->shift_switch)));
}

void smtk_app_win_toggle_mouse_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->mouse_switch))
		gtk_switch_set_active(
			GTK_SWITCH(win->mouse_switch),
			!gtk_switch_get_active(GTK_SWITCH(win->mouse_switch)));
}

void smtk_app_win_toggle_border_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->border_switch))
		gtk_switch_set_active(
			GTK_SWITCH(win->border_switch),
			!gtk_switch_get_active(GTK_SWITCH(win->border_switch)));
}

void smtk_app_win_set_size(SmtkAppWin *win, int width, int height)
{
	g_return_if_fail(win != NULL);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(win->width_entry), width);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(win->height_entry), height);
}

void smtk_app_win_show_usage_dialog(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	AdwDialog *dialog = adw_alert_dialog_new(
		_("Usage"),
		_("1. Please input admin password after toggling the switch, "
		  "because it needs superuser permission to read input events, "
		  "and Wayland does not allow running graphics program with "
		  "superuser permission, so it uses polkit to run a backend "
		  "with superuser permission. This program does not handle or "
		  "store your password. Users in `wheel` group can skip "
		  "password authentication.\n\n"
		  "2. After you toggle the switch to show the floating window, "
		  "you need to drag it manually to anywhere you want, "
		  "because Wayland does not allow window to set its position. "
		  "The \"Clickable\" label on titlebar can be dragged as a "
		  "handle.\n\n"
		  "3. Because Wayland does not allow a window to set "
		  "\"Always on Top\" and \"Always on Visible Workspace\" "
		  "by itself, you should set it manually if you are in a "
		  "Wayland session and your window manager support it.\n"
		  "For example if you are using GNOME Shell (Wayland), you can "
		  "right click the \"Clickable\" on title bar to show a window "
		  "manager menu and check \"Always on Top\" and \"Always on "
		  "Visible Workspace\" in it.\n"
		  "If you are using KDE Plasma (Wayland), you can right click "
		  "\"Floating Window - Show Me The Key\" on task bar, check "
		  "\"Move to Desktop\" -> \"All Desktops\" and "
		  "\"More Actions\" -> \"Keep Above Others\".\n"
		  "You can check this project's <a "
		  "href=\"https://github.com/AlynxZhou/showmethekey#special-"
		  "notice-for-wayland-session-users\">README</a> to see if "
		  "there are configurations for your compositor.\n\n"
		  "4. To allow user move or resize the keys window, it is not "
		  "click through by default, after moving it to the location "
		  "you want, turn off \"Clickable\" switch so it won't block "
		  "your other operations.\n\n"
		  "5. If you want to pause it (for example you need to insert "
		  "password), you can use the \"Pause\" switch, it will not "
		  "record your keys when paused.\n\n"
		  "6. Set Timeout to 0 if you want to keep all keys.\n\n"
		  "You can open this dialog again via menu icon on title bar "
		  "-> \"Usage\"."));
	adw_alert_dialog_set_body_use_markup(ADW_ALERT_DIALOG(dialog), true);
	adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "close",
				      _("Close"));
	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog),
					      "close");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
	adw_dialog_present(dialog, GTK_WIDGET(win));
}

void smtk_app_win_show_about_dialog(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

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

	adw_show_about_dialog(GTK_WIDGET(win), "developers", developers,
			      "artists", artists, "documenters", documenters,
			      "translator-credits", _("translator-credits"),
			      "title", _("About Show Me The Key"),
			      "application-name", _("Show Me The Key"),
			      "comments", _("Show keys you typed on screen."),
			      "copyright", "Copyright © 2021-2022 Alynx Zhou",
			      "license", license, "application-icon",
			      "one.alynx.showmethekey", "website",
			      "https://showmethekey.alynx.one/", "version",
			      PROJECT_VERSION, NULL);
}
