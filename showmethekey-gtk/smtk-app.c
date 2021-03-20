#include <gtk/gtk.h>

#include "smtk-app.h"
#include "smtk-app-win.h"

struct _SmtkApp {
	GtkApplication parent_instance;
};
G_DEFINE_TYPE(SmtkApp, smtk_app, GTK_TYPE_APPLICATION)

static void smtk_app_init(SmtkApp *app)
{
}

static void smtk_app_activate(GApplication *app)
{
	GtkWidget *win = smtk_app_win_new(SMTK_APP(app));
	gtk_window_present(GTK_WINDOW(win));
}

static void smtk_app_class_init(SmtkAppClass *app_class)
{
	G_APPLICATION_CLASS(app_class)->activate = smtk_app_activate;
}

SmtkApp *smtk_app_new(void)
{
	return g_object_new(SMTK_TYPE_APP,
			    "application-id", "one.alynx.showmethekey",
			    "flags", G_APPLICATION_HANDLES_OPEN,
			    NULL);
}
