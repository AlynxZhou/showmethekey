#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "smtk.h"
#include "smtk-app.h"
#include "smtk-app-win.h"

struct _SmtkApp {
	AdwApplication parent_instance;
	GtkWidget *win;
	bool keys_win_opt;
	bool app_win_opt;
	bool clickable_opt;
};
G_DEFINE_TYPE(SmtkApp, smtk_app, ADW_TYPE_APPLICATION)

static void clickable_action(GSimpleAction *action, GVariant *parameter,
			     gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_clickable_switch(SMTK_APP_WIN(app->win));
}

static void pause_action(GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_pause_switch(SMTK_APP_WIN(app->win));
}

static void shift_action(GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_shift_switch(SMTK_APP_WIN(app->win));
}

static void mouse_action(GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_mouse_switch(SMTK_APP_WIN(app->win));
}

static void border_action(GSimpleAction *action, GVariant *parameter,
			  gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_border_switch(SMTK_APP_WIN(app->win));
}

static void hide_visible_action(GSimpleAction *action, GVariant *parameter,
				gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_hide_visible_switch(SMTK_APP_WIN(app->win));
}

static void usage_action(GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_show_usage_dialog(SMTK_APP_WIN(app->win));
}

static void about_action(GSimpleAction *action, GVariant *parameter,
			 gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_show_about_dialog(SMTK_APP_WIN(app->win));
}

static void quit_action(GSimpleAction *action, GVariant *parameter,
			gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL) {
		gtk_window_destroy(GTK_WINDOW(app->win));
		app->win = NULL;
	}

	g_application_quit(G_APPLICATION(app));
}

static void smtk_app_init(SmtkApp *app)
{
	app->win = NULL;
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
		  N_("Show keys window on start up (deprecated by `-k, --show-keys-win`)."),
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
	// Application is already single instance.
	// We use this to prevent mutliply windows.
	SmtkApp *app = SMTK_APP(g_app);

	if (app->win == NULL) {
		app->win = smtk_app_win_new(SMTK_APP(app));
		g_debug("Keys win: %s.", app->keys_win_opt ? "true" : "false");
		g_debug("App win: %s.", app->app_win_opt ? "true" : "false");
		g_debug("Clickable: %s.",
			app->clickable_opt ? "true" : "false");
		// By default keys win is clickable.
		if (!app->clickable_opt)
			smtk_app_win_toggle_clickable_switch(
				SMTK_APP_WIN(app->win));
		if (app->app_win_opt)
			gtk_window_present(GTK_WINDOW(app->win));
		if (app->keys_win_opt)
			smtk_app_win_activate(SMTK_APP_WIN(app->win));
	}
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

static void smtk_app_class_init(SmtkAppClass *app_class)
{
	GApplicationClass *g_app_class = G_APPLICATION_CLASS(app_class);

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
