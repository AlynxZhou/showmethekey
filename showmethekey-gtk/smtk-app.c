#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "config.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"

struct _SmtkApp {
	AdwApplication parent_instance;
	GSettings *settings;
	GtkWidget *app_win;
	GtkWidget *keys_win;
	bool keys_win_opt;
	bool app_win_opt;
	bool clickable_opt;
};
G_DEFINE_TYPE(SmtkApp, smtk_app, ADW_TYPE_APPLICATION)

static void clickable_action(GSimpleAction *action, GVariant *parameter,
			     void *data)
{
	SmtkApp *app = data;

	const char *key = "clickable";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void pause_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	const char *key = "paused";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void shift_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	const char *key = "show-shift";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void keyboard_action(GSimpleAction *action, GVariant *parameter,
			    void *data)
{
	SmtkApp *app = data;

	const char *key = "show-keyboard";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void mouse_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	const char *key = "show-mouse";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void border_action(GSimpleAction *action, GVariant *parameter,
			  void *data)
{
	SmtkApp *app = data;

	const char *key = "show-border";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void hide_visible_action(GSimpleAction *action, GVariant *parameter,
				void *data)
{
	SmtkApp *app = data;

	const char *key = "hide-visible";
	const bool value = g_settings_get_boolean(app->settings, key);
	g_settings_set_boolean(app->settings, key, !value);
}

static void usage_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	if (app->app_win != NULL)
		smtk_app_win_show_usage(SMTK_APP_WIN(app->app_win));
}

static void about_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	if (app->app_win != NULL)
		smtk_app_win_show_about(SMTK_APP_WIN(app->app_win));
}

static void quit_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *app = data;

	smtk_app_quit(app);
}

static void on_app_win_destroy(SmtkApp *app, GtkWidget *app_win)
{
	app->app_win = NULL;
}

static void on_keys_win_destroy(SmtkApp *app, GtkWidget *keys_win)
{
	app->keys_win = NULL;
}

static void show_keys_win(SmtkApp *app)
{
	if (app->keys_win == NULL) {
		g_autoptr(GError) error = NULL;
		app->keys_win =
			smtk_keys_win_new(app, app->clickable_opt, &error);
		if (error != NULL) {
			g_warning("Failed to create keys win: %s.",
				  error->message);
			return;
		}
		g_signal_connect_swapped(app->keys_win, "destroy",
					 G_CALLBACK(on_keys_win_destroy), app);
		gtk_window_present(GTK_WINDOW(app->keys_win));
	}
}

static void show_app_win(SmtkApp *app)
{
	if (app->app_win == NULL) {
		app->app_win = smtk_app_win_new(app);
		g_signal_connect_swapped(app->app_win, "destroy",
					 G_CALLBACK(on_app_win_destroy), app);
		gtk_window_present(GTK_WINDOW(app->app_win));
	}
}

static void on_active_changed(SmtkApp *app, char *key, GSettings *settings)
{
	if (g_settings_get_boolean(app->settings, "active"))
		show_keys_win(app);
	else if (app->keys_win != NULL)
		gtk_window_destroy(GTK_WINDOW(app->keys_win));
}

static void smtk_app_init(SmtkApp *app)
{
	app->settings = NULL;
	app->keys_win = NULL;
	app->app_win = NULL;
	app->keys_win_opt = false;
	app->app_win_opt = true;
	app->clickable_opt = true;

	g_set_application_name(_("Show Me The Key"));
	gtk_window_set_default_icon_name("one.alynx.showmethekey");

	const GOptionEntry options[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Display version then exit."), NULL },
		// Keep this option because I don't want to break CLI.
		{ "active", 'a', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Show keys window on start up (deprecated by `-k, --keys-win`)."),
		  NULL },
		{ "keys-win", 'k', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Show keys window on start up."), NULL },
		{ "no-app-win", 'A', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Hide app window and show keys window on start up."),
		  NULL },
		{ "no-clickable", 'C', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Make keys window unclickable on start up."), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	g_application_add_main_option_entries(G_APPLICATION(app), options);
}

static void smtk_app_activate(GApplication *g_app)
{
	// Application is already single instance, and we use this to prevent
	// mutliply windows.
	SmtkApp *app = SMTK_APP(g_app);

	g_debug("Keys win: %s.", app->keys_win_opt ? "true" : "false");
	g_debug("App win: %s.", app->app_win_opt ? "true" : "false");
	g_debug("Clickable: %s.", app->clickable_opt ? "true" : "false");

	if (app->keys_win_opt)
		show_keys_win(app);
	if (app->app_win_opt)
		show_app_win(app);
}

static void smtk_app_startup(GApplication *g_app)
{
	SmtkApp *app = SMTK_APP(g_app);

	// Because application is not a construct property of GtkWindow,
	// we have to setup accels here.
	GActionEntry actions[] = {
		{ "clickable", clickable_action, NULL, NULL, NULL },
		{ "pause", pause_action, NULL, NULL, NULL },
		{ "shift", shift_action, NULL, NULL, NULL },
		{ "keyboard", keyboard_action, NULL, NULL, NULL },
		{ "mouse", mouse_action, NULL, NULL, NULL },
		{ "border", border_action, NULL, NULL, NULL },
		{ "hide-visible", hide_visible_action, NULL, NULL, NULL },
		{ "usage", usage_action, NULL, NULL, NULL },
		{ "about", about_action, NULL, NULL, NULL },
		{ "quit", quit_action, NULL, NULL, NULL }
	};
	g_action_map_add_action_entries(G_ACTION_MAP(app), actions,
					G_N_ELEMENTS(actions), app);
	const char *clickable_accels[] = { "<Ctrl>C", NULL };
	const char *pause_accels[] = { "<Ctrl>P", NULL };
	const char *shift_accels[] = { "<Ctrl>S", NULL };
	const char *keyboard_accels[] = { "<Ctrl>K", NULL };
	const char *mouse_accels[] = { "<Ctrl>M", NULL };
	const char *border_accels[] = { "<Ctrl>B", NULL };
	const char *hide_visible_accels[] = { "<Ctrl>V", NULL };
	const char *usage_accels[] = { "<Ctrl>U", NULL };
	const char *about_accels[] = { "<Ctrl>A", NULL };
	const char *quit_accels[] = { "<Ctrl>Q", NULL };
	// See Description of
	// <https://developer.gnome.org/gio/stable/GActionMap.html>
	// about "app." here.
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(app), "app.clickable", clickable_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.pause",
					      pause_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.shift",
					      shift_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app),
					      "app.keyboard", keyboard_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.mouse",
					      mouse_accels);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(app), "app.hide-visible", hide_visible_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app),
					      "app.border", border_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.usage",
					      usage_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.about",
					      about_accels);
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit",
					      quit_accels);

	G_APPLICATION_CLASS(smtk_app_parent_class)->startup(g_app);
}

// See <https://developer.gnome.org/gio/stable/GApplication.html#GApplication-handle-local-options>.
static int smtk_app_handle_local_options(GApplication *application,
					 GVariantDict *options)
{
	SmtkApp *app = SMTK_APP(application);

	if (g_variant_dict_contains(options, "version")) {
		g_print(PROJECT_VERSION "\n");
		return 0;
	}

	if (g_variant_dict_contains(options, "active")) {
		g_warning("`-a, --active` is deprecated by `-k, --keys-win`.");
		app->keys_win_opt = true;
	}
	if (g_variant_dict_contains(options, "keys-win"))
		app->keys_win_opt = true;
	if (g_variant_dict_contains(options, "no-app-win")) {
		app->app_win_opt = false;
		// No one wants to run it with no window, hiding app window
		// means they want keys window directly.
		app->keys_win_opt = true;
	}
	if (g_variant_dict_contains(options, "no-clickable"))
		app->clickable_opt = false;

	return -1;
}

static void smtk_app_constructed(GObject *object)
{
	SmtkApp *app = SMTK_APP(object);

	app->settings = g_settings_new("one.alynx.showmethekey");
	g_signal_connect_swapped(app->settings, "changed::active",
				 G_CALLBACK(on_active_changed), app);

	G_OBJECT_CLASS(smtk_app_parent_class)->constructed(object);
}

static void smtk_app_dispose(GObject *object)
{
	SmtkApp *app = SMTK_APP(object);

	g_clear_object(&app->settings);

	G_OBJECT_CLASS(smtk_app_parent_class)->dispose(object);
}

static void smtk_app_class_init(SmtkAppClass *app_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(app_class);
	GApplicationClass *g_app_class = G_APPLICATION_CLASS(app_class);

	object_class->constructed = smtk_app_constructed;
	object_class->dispose = smtk_app_dispose;

	g_app_class->activate = smtk_app_activate;
	g_app_class->startup = smtk_app_startup;

	// See <https://developer.gnome.org/CommandLine/>.
	g_app_class->handle_local_options = smtk_app_handle_local_options;
}

SmtkApp *smtk_app_new(void)
{
	return g_object_new(SMTK_TYPE_APP, "application-id",
			    "one.alynx.showmethekey", NULL);
}

void smtk_app_quit(SmtkApp *app)
{
	g_return_if_fail(app != NULL);

	if (app->app_win != NULL) {
		gtk_window_destroy(GTK_WINDOW(app->app_win));
		app->app_win = NULL;
	}

	if (app->keys_win != NULL) {
		gtk_window_destroy(GTK_WINDOW(app->keys_win));
		app->keys_win = NULL;
	}

	g_application_quit(G_APPLICATION(app));
}
