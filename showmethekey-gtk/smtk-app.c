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

static void smtk_app_init(SmtkApp *app)
{
	app->win = NULL;

	g_set_application_name(_("Show Me The Key"));
	gtk_window_set_default_icon_name("showmethekey");

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

// See <https://developer.gnome.org/gio/stable/GApplication.html#GApplication-handle-local-options>.
static gint smtk_app_handle_local_options(GApplication *application,
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

	// See <https://developer.gnome.org/CommandLine/>.
	g_app_class->handle_local_options = smtk_app_handle_local_options;
}

SmtkApp *smtk_app_new(void)
{
	return g_object_new(SMTK_TYPE_APP, "application-id",
			    "one.alynx.showmethekey", NULL);
}
