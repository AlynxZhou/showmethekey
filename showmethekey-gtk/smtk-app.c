#include <gtk/gtk.h>

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
}

static void smtk_app_activate(GApplication *gapp)
{
	// Application is already single instance.
	// We use this to prevent mutliply windows.
	SmtkApp *app = SMTK_APP(gapp);
	if (app->win == NULL) {
		app->win = smtk_app_win_new(SMTK_APP(app));
		gtk_window_present(GTK_WINDOW(app->win));
	}
}

static void smtk_app_class_init(SmtkAppClass *app_class)
{
	G_APPLICATION_CLASS(app_class)->activate = smtk_app_activate;
}

SmtkApp *smtk_app_new(void)
{
	return g_object_new(SMTK_TYPE_APP, "application-id",
			    "one.alynx.showmethekey", NULL);
}
