#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "smtk.h"
#include "smtk-app.h"
#include "smtk-app-win.h"

struct _SmtkApp {
	GtkApplication parent_instance;
	GtkWidget *win;
};
G_DEFINE_TYPE(SmtkApp, smtk_app, GTK_TYPE_APPLICATION)

static void hide_action(GSimpleAction *action, GVariant *parameter,
			gpointer user_data)
{
	SmtkApp *app = SMTK_APP(user_data);

	if (app->win != NULL)
		smtk_app_win_toggle_hide_switch(SMTK_APP_WIN(app->win));
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

	g_clear_pointer((GtkWindow **)&app->win, gtk_window_destroy);

	g_application_quit(G_APPLICATION(app));
}

static void smtk_app_init(SmtkApp *app)
{
	app->win = NULL;

	g_set_application_name(_("Show Me The Key"));
	gtk_window_set_default_icon_name("one.alynx.showmethekey");

	const GOptionEntry options[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, NULL,
		  N_("Display version then exit."), NULL },
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
		gtk_window_present(GTK_WINDOW(app->win));
	}
}

static void smtk_app_startup(GApplication *g_app)
{
	SmtkApp *app = SMTK_APP(g_app);

	// Because application is not a construct property of GtkWindow,
	// we have to setup accels here.
	GActionEntry actions[] = { { "hide", hide_action, NULL, NULL, NULL },
				   { "usage", usage_action, NULL, NULL, NULL },
				   { "about", about_action, NULL, NULL, NULL },
				   { "quit", quit_action, NULL, NULL, NULL } };
	g_action_map_add_action_entries(G_ACTION_MAP(app), actions,
					G_N_ELEMENTS(actions), app);
	const char *hide_accels[] = { "<Ctrl>H", NULL };
	const char *usage_accels[] = { "<Ctrl>U", NULL };
	const char *about_accels[] = { "<Ctrl>A", NULL };
	const char *quit_accels[] = { "<Ctrl>Q", NULL };
	// See Description of
	// <https://developer.gnome.org/gio/stable/GActionMap.html>
	// about "app." here.
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.hide",
					      hide_accels);
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
	if (g_variant_dict_contains(options, "version")) {
		g_print(PROJECT_VERSION "\n");
		return 0;
	}

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
