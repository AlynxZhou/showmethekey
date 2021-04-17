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
	GtkWidget *mode_selector;
	GtkWidget *width_entry;
	GtkWidget *height_entry;
	GtkWidget *keys_win;
};
G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, GTK_TYPE_APPLICATION_WINDOW)

static void smtk_app_win_enable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->hide_switch, FALSE);
	gtk_widget_set_sensitive(win->mode_selector, TRUE);
	gtk_widget_set_sensitive(win->width_entry, TRUE);
	gtk_widget_set_sensitive(win->height_entry, TRUE);
}

static void smtk_app_win_disable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->hide_switch, TRUE);
	gtk_widget_set_sensitive(win->mode_selector, FALSE);
	gtk_widget_set_sensitive(win->width_entry, FALSE);
	gtk_widget_set_sensitive(win->height_entry, FALSE);
}

static void smtk_app_win_keys_win_on_destroy(SmtkAppWin *win,
					     gpointer user_data,
					     SmtkKeysWin *keys_win)
{
	if (win->keys_win != NULL) {
		win->keys_win = NULL;
		gtk_switch_set_active(GTK_SWITCH(win->keys_win_switch), FALSE);
		gtk_switch_set_active(GTK_SWITCH(win->hide_switch), FALSE);
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
			const char *mode_id = gtk_combo_box_get_active_id(
				GTK_COMBO_BOX(win->mode_selector));
			g_debug("Mode: %s.", mode_id);
			SmtkKeyMode mode = SMTK_KEY_MODE_COMPOSED;
			if (strcmp(mode_id, "raw") == 0)
				mode = SMTK_KEY_MODE_RAW;
			gint width = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(win->width_entry));
			width = width <= 0 ? 1500 : width;
			gint height = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(win->height_entry));
			height = height <= 0 ? 200 : height;
			g_debug("Size: %dx%d.", width, height);
			GError *error = NULL;
			win->keys_win =
				smtk_keys_win_new(mode, width, height, &error);
			if (win->keys_win == NULL) {
				g_warning(error->message);
				g_error_free(error);
				gtk_switch_set_active(
					GTK_SWITCH(win->keys_win_switch),
					FALSE);
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
			gtk_widget_destroy(GTK_WIDGET(win->keys_win));
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
	// GtkWidget *switch_label = gtk_label_new(_("Start"));
	// gtk_widget_set_hexpand(switch_label, FALSE);
	// gtk_widget_set_parent(switch_label, GTK_WIDGET(win));
	// win->canvas_switch = gtk_switch_new();
	// gtk_widget_set_hexpand(win->canvas_switch, FALSE);
	// // In GTK4 we can just add children here.
	// gtk_widget_set_parent(win->canvas_switch, GTK_WIDGET(win));

	gtk_widget_init_template(GTK_WIDGET(win));
	GtkBuilder *builder = gtk_builder_new_from_resource(
		"/one/alynx/showmethekey/smtk-app-win-menu.ui");
	GMenuModel *menu_model =
		G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(win->menu_button),
				       menu_model);
	g_object_unref(builder);

	// gtk_widget_grab_focus(win->keys_win_switch);
	// See <https://stackoverflow.com/questions/8728172/gtk-set-default-button-for-dialog>.
	// The comment said that to use gtk_window_set_default(),
	// you need to clear focus first.
	// And for GTK4 use gtk_window_set_default_widget().
	gtk_window_set_focus(GTK_WINDOW(win), NULL);
	gtk_window_set_default(GTK_WINDOW(win), win->keys_win_switch);

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->width_entry), 0,
				  INT_MAX);
	// FIXME: Page increment works wrongly, consider a GTK problem.
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->width_entry), 100,
				       500);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(win->height_entry), 0,
				  INT_MAX);
	// FIXME: Page increment works wrongly, consider a GTK problem.
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(win->height_entry), 100,
				       500);

	win->settings = g_settings_new("one.alynx.showmethekey");
	// Though gschema's enum has a int value property,
	// it uses string nick property for binding.
	g_settings_bind(win->settings, "mode", win->mode_selector, "active_id",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "width", win->width_entry, "value",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "height", win->height_entry, "value",
			G_SETTINGS_BIND_DEFAULT);

	if (g_settings_get_boolean(win->settings, "first-time")) {
		smtk_app_win_show_usage_dialog(win);
		g_settings_set_boolean(win->settings, "first-time", FALSE);
	}
}

static void smtk_app_win_dispose(GObject *object)
{
	SmtkAppWin *win = SMTK_APP_WIN(object);

	// g_clear_pointer(&win->canvas_switch, gtk_widget_unparent);

	if (win->settings != NULL) {
		g_object_unref(win->settings);
		win->settings = NULL;
	}

	// Manually destroy keys_win, so CLI backend will be told to stop.
	if (win->keys_win != NULL)
		gtk_widget_destroy(win->keys_win);

	G_OBJECT_CLASS(smtk_app_win_parent_class)->dispose(object);
}

static void smtk_app_win_class_init(SmtkAppWinClass *win_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(win_class);

	// object_class->constructed = smtk_app_win_constructed;
	// In GTK4, we need to free children which is added in code in dispose.
	object_class->dispose = smtk_app_win_dispose;

	// // In GTK4, we need to set a layout manager to add childrens in code.
	// gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(win_class),
	// 					 GTK_TYPE_BOX_LAYOUT);

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
					     SmtkAppWin, mode_selector);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, width_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, height_entry);
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

	GtkWidget *dialog = gtk_message_dialog_new(
		GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
		GTK_BUTTONS_CLOSE,
		_("Usage:\n\n"
		  "1. Please input your password after toggling the switch, "
		  "because it needs superuser permission to read input events, "
		  "and Wayland does not allow running graphics program with "
		  "superuser permission, so it uses polkit to run a backend "
		  "with superuser permission. This program does not handle or "
		  "store your password.\n\n"
		  "2. After you toggle the switch to show the floating window, "
		  "you need to drag it manually to anywhere you want, "
		  "because Wayland does not allow window to set its position. "
		  "Though the floating window is mostly transparent for click, "
		  "the \"Clickable Area\" label on titlebar are clickable and "
		  "can be dragged as a handle.\n\n"
		  "3. Because Wayland does not allow a window to set "
		  "\"Always on Top\" and \"Always on Visible Workspace\" "
		  "by itself, you should set it manually if you are in a "
		  "Wayland session and your window manager support it. "
		  "For example if you are using GNOME Shell, you can right "
		  "click the \"Clickable Area\" in titlebar to show a window "
		  "manager menu and check \"Always on Top\" and \"Always on "
		  "Visible Workspace\" in it.\n\n"
		  "4. If you want to temporary pause it (for example you need "
		  "to insert password), you can use the \"Temporary Hide\" "
		  "switch, it will not record your keys while hiding.\n\n"
		  "You can open this dialog again from Menu -> Usage."));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void smtk_app_win_show_about_dialog(SmtkAppWin *win)
{
	g_return_if_fail(win != NULL);

	const gchar *authors[] = { "Alynx Zhou", "LGiki", NULL };

	const gchar *artists[] = { "Freepik", NULL };

	const gchar *documenters[] = { "Alynx Zhou", NULL };

	gtk_show_about_dialog(
		GTK_WINDOW(win), "authors", authors, "artists", artists,
		"documenters", documenters, "translator-credits",
		_("translator-credits"), "title", _("About Show Me The Key"),
		"program-name", _("Show Me The Key"), "comments",
		_("Show keys you typed on screen."), "copyright",
		"Copyright © 2021 Alynx Zhou", "license-type",
		GTK_LICENSE_APACHE_2_0, "logo-icon-name", "showmethekey",
		"website", "https://showmethekey.alynx.one/", "website-label",
		"showmethekey.alynx.one", "version", PROJECT_VERSION, NULL);
}
