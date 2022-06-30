#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "smtk.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"
#include "smtk-keys-emitter.h"

struct _SmtkAppWin {
	GtkApplicationWindow parent_instance;
	GSettings *settings;
	GtkWidget *menu_button;
	GtkWidget *keys_win_switch;
	GtkWidget *hide_switch;
	GtkWidget *mouse_switch;
	GtkWidget *mode_selector;
	GtkWidget *width_entry;
	GtkWidget *height_entry;
	GtkWidget *timeout_entry;
	GtkWidget *keys_win;
};
G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, GTK_TYPE_APPLICATION_WINDOW)

static void smtk_app_win_enable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->hide_switch, false);
	gtk_widget_set_sensitive(win->mouse_switch, true);
	gtk_widget_set_sensitive(win->mode_selector, true);
	gtk_widget_set_sensitive(win->width_entry, true);
	gtk_widget_set_sensitive(win->height_entry, true);
	gtk_widget_set_sensitive(win->timeout_entry, true);
}

static void smtk_app_win_disable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->hide_switch, true);
	gtk_widget_set_sensitive(win->mouse_switch, false);
	gtk_widget_set_sensitive(win->mode_selector, false);
	gtk_widget_set_sensitive(win->width_entry, false);
	gtk_widget_set_sensitive(win->height_entry, false);
	gtk_widget_set_sensitive(win->timeout_entry, false);
}

static void smtk_app_win_keys_win_on_destroy(SmtkAppWin *win,
					     gpointer user_data,
					     SmtkKeysWin *keys_win)
{
	if (win->keys_win != NULL) {
		win->keys_win = NULL;
		gtk_switch_set_active(GTK_SWITCH(win->keys_win_switch), false);
		gtk_switch_set_active(GTK_SWITCH(win->hide_switch), false);
		smtk_app_win_enable(win);
	}
}

// See <https://mail.gnome.org/archives/networkmanager-list/2010-October/msg00129.html>.
// notify of property have one more argument for property
// in the middle of instance and object.
static void smtk_app_win_on_keys_win_switch_active(SmtkAppWin *win,
						   GParamSpec *prop,
						   GtkSwitch *keys_win_switch)
{
	if (gtk_switch_get_active(GTK_SWITCH(win->keys_win_switch))) {
		if (win->keys_win == NULL) {
			bool show_mouse = gtk_switch_get_active(
				GTK_SWITCH(win->mouse_switch));
			const char *mode_id = gtk_combo_box_get_active_id(
				GTK_COMBO_BOX(win->mode_selector));
			g_debug("Mode: %s.", mode_id);
			SmtkKeyMode mode = SMTK_KEY_MODE_COMPOSED;
			if (strcmp(mode_id, "raw") == 0)
				mode = SMTK_KEY_MODE_RAW;
			int width = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(win->width_entry));
			width = width <= 0 ? 1500 : width;
			int height = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(win->height_entry));
			int timeout = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(win->timeout_entry));
			height = height <= 0 ? 200 : height;
			g_debug("Size: %dx%d.", width, height);
			GError *error = NULL;
			win->keys_win = smtk_keys_win_new(win, show_mouse, mode,
							  width, height,
							  timeout, &error);
			if (win->keys_win == NULL) {
				g_warning("%s", error->message);
				g_error_free(error);
				gtk_switch_set_active(
					GTK_SWITCH(win->keys_win_switch),
					false);
				return;
			}
			smtk_app_win_disable(win);
			g_signal_connect_object(
				win->keys_win, "destroy",
				G_CALLBACK(smtk_app_win_keys_win_on_destroy),
				win, G_CONNECT_SWAPPED);
			gtk_window_present(GTK_WINDOW(win->keys_win));
		}
	} else {
		// We clear pointer and change widget states in signal callback.
		if (win->keys_win != NULL)
			gtk_window_destroy(GTK_WINDOW(win->keys_win));
	}
}

static void smtk_app_win_on_hide_switch_active(SmtkAppWin *win,
					       GParamSpec *prop,
					       GtkSwitch *hide_switch)
{
	// This only works when keys_win is open.
	// We don't disable or turn off it here,
	// doing this when keys_win destroyed is enough.
	if (win->keys_win == NULL)
		return;

	if (gtk_switch_get_active(GTK_SWITCH(win->hide_switch)))
		smtk_keys_win_hide(SMTK_KEYS_WIN(win->keys_win));
	else
		smtk_keys_win_show(SMTK_KEYS_WIN(win->keys_win));
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

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->width_entry), 0,
				  INT_MAX);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->width_entry), 100,
				       500);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->height_entry), 0,
				  INT_MAX);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->height_entry), 100,
				       500);

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->timeout_entry), 0,
				  30000);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->timeout_entry), 100,
				       1000);

	win->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(win->settings, "show-mouse", win->mouse_switch,
			"active", G_SETTINGS_BIND_DEFAULT);
	// Though gschema's enum has a int value property,
	// it uses string nick property for binding.
	g_settings_bind(win->settings, "mode", win->mode_selector, "active_id",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "width", win->width_entry, "value",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "height", win->height_entry, "value",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "timeout", win->timeout_entry, "value",
			G_SETTINGS_BIND_DEFAULT);

	if (g_settings_get_boolean(win->settings, "first-time")) {
		smtk_app_win_show_usage_dialog(win);
		g_settings_set_boolean(win->settings, "first-time", false);
	}
}

static void smtk_app_win_dispose(GObject *object)
{
	SmtkAppWin *win = SMTK_APP_WIN(object);

	g_clear_pointer(&win->settings, g_object_unref);

	// Manually destroy keys_win, so CLI backend will be told to stop.
	g_clear_pointer((GtkWindow **)&win->keys_win, gtk_window_destroy);

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
					     SmtkAppWin, hide_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, mouse_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, mode_selector);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, width_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, height_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, timeout_entry);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_keys_win_switch_active);
	gtk_widget_class_bind_template_callback(
		GTK_WIDGET_CLASS(win_class),
		smtk_app_win_on_hide_switch_active);
}

GtkWidget *smtk_app_win_new(SmtkApp *app)
{
	return g_object_new(SMTK_TYPE_APP_WIN, "application", app, NULL);
}

void smtk_app_win_toggle_hide_switch(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	if (gtk_widget_get_sensitive(win->hide_switch))
		gtk_switch_set_active(
			GTK_SWITCH(win->hide_switch),
			!gtk_switch_get_active(GTK_SWITCH(win->hide_switch)));
}

void smtk_app_win_show_usage_dialog(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	GtkWidget *dialog = gtk_message_dialog_new_with_markup(
		GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
		GTK_BUTTONS_CLOSE,
		_("Usage:\n\n"
		  "1. Please input admin password after toggling the switch, "
		  "because it needs superuser permission to read input events, "
		  "and Wayland does not allow running graphics program with "
		  "superuser permission, so it uses polkit to run a backend "
		  "with superuser permission. This program does not handle or "
		  "store your password. Users in `wheel` group can skip "
		  "password authentication.\n\n"
		  "2. After you toggle the switch to show the floating window, "
		  "you need to drag it manually to anywhere you want, "
		  "because Wayland does not allow window to set its position. "
		  "Though the floating window is mostly transparent for click, "
		  "the \"Clickable Area\" label on titlebar are clickable and "
		  "can be dragged as a handle.\n\n"
		  "3. Because Wayland does not allow a window to set "
		  "\"Always on Top\" and \"Always on Visible Workspace\" "
		  "by itself, you should set it manually if you are in a "
		  "Wayland session and your window manager support it.\n"
		  "For example if you are using GNOME Shell (Wayland), you can "
		  "right click the \"Clickable Area\" on title bar to show a "
		  "window manager menu and check \"Always on Top\" and "
		  "\"Always on Visible Workspace\" in it.\n"
		  "If you are using KDE Plasma (Wayland), you can right click "
		  "\"Floating Window - Show Me The Key\" on task bar, check "
		  "\"Move to Desktop\" -> \"All Desktops\" and "
		  "\"More Actions\" -> \"Keep Above Others\".\n"
		  "You can check this project's <a "
		  "href=\"https://github.com/AlynxZhou/showmethekey#special-"
		  "notice-for-wayland-session-users\">README</a> to see if "
		  "there are configurations for your compositor.\n\n"
		  "4. If you want to temporary pause it (for example you need "
		  "to insert password), you can use the \"Temporary Hide\" "
		  "switch, it will not record your keys while hiding.\n\n"
		  "5. Set Timeout to 0 if you want to keep all keys.\n\n"
		  "You can open this dialog again via menu icon on title bar "
		  "-> \"Usage\"."));
	gtk_window_set_modal(GTK_WINDOW(dialog), true);
	gtk_window_present(GTK_WINDOW(dialog));
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gtk_window_destroy), dialog);
}

void smtk_app_win_show_about_dialog(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	const char *authors[] = { "Alynx Zhou", "LGiki", NULL };

	const char *artists[] = { "Freepik", NULL };

	const char *documenters[] = { "Alynx Zhou", NULL };

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

	gtk_show_about_dialog(
		GTK_WINDOW(win), "authors", authors, "artists", artists,
		"documenters", documenters, "translator-credits",
		_("translator-credits"), "title", _("About Show Me The Key"),
		"program-name", _("Show Me The Key"), "comments",
		_("Show keys you typed on screen."), "copyright",
		"Copyright © 2021-2022 Alynx Zhou", "license", license,
		"logo-icon-name", "one.alynx.showmethekey", "website",
		"https://showmethekey.alynx.one/", "website-label",
		"showmethekey.alynx.one", "version", PROJECT_VERSION, NULL);
}
