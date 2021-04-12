#include <gtk/gtk.h>

#include "smtk.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"
#include "smtk-keys-emitter.h"

struct _SmtkAppWin {
	GtkApplicationWindow parent_instance;
	GSettings *settings;
	GtkWidget *keys_win_switch;
	GtkWidget *mode_selector;
	GtkWidget *width_entry;
	GtkWidget *height_entry;
	GtkWidget *keys_win;
};
G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, GTK_TYPE_APPLICATION_WINDOW)

// TODO: Internationalization.

static void smtk_app_win_enable(SmtkAppWin *win)
{
	gtk_widget_set_sensitive(win->mode_selector, TRUE);
	gtk_widget_set_sensitive(win->width_entry, TRUE);
	gtk_widget_set_sensitive(win->height_entry, TRUE);
}

static void smtk_app_win_disable(SmtkAppWin *win)
{
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
		smtk_app_win_enable(win);
	}
}

// See <https://mail.gnome.org/archives/networkmanager-list/2010-October/msg00129.html>.
// notify of property have one more argument for property
// in the middle of instance and object.
static void smtk_app_win_on_switch_active(SmtkAppWin *win, GParamSpec *prop,
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
		if (win->keys_win != NULL) {
			gtk_widget_destroy(GTK_WIDGET(win->keys_win));
			win->keys_win = NULL;
			smtk_app_win_enable(win);
		}
	}
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
}

static void smtk_app_win_dispose(GObject *object)
{
	SmtkAppWin *win = SMTK_APP_WIN(object);

	// g_clear_pointer(&win->canvas_switch, gtk_widget_unparent);

	if (win->settings != NULL) {
		g_object_unref(win->settings);
		win->settings = NULL;
	}

	G_OBJECT_CLASS(smtk_app_win_parent_class)->dispose(object);
}

static void smtk_app_win_class_init(SmtkAppWinClass *win_class)
{
	// In GTK4, we need to free children which is added in code in dispose.
	G_OBJECT_CLASS(win_class)->dispose = smtk_app_win_dispose;

	// // In GTK4, we need to set a layout manager to add childrens in code.
	// gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(win_class),
	// 					 GTK_TYPE_BOX_LAYOUT);

	gtk_widget_class_set_template_from_resource(
		GTK_WIDGET_CLASS(win_class),
		"/one/alynx/showmethekey/smtk-app-win.ui");
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, keys_win_switch);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, mode_selector);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, width_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class),
					     SmtkAppWin, height_entry);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(win_class),
						smtk_app_win_on_switch_active);
}

GtkWidget *smtk_app_win_new(SmtkApp *app)
{
	return g_object_new(SMTK_TYPE_APP_WIN, "application", app, NULL);
}
