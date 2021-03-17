#include <gtk/gtk.h>

#include "smtk-app.h"
#include "smtk-app-win.h"

struct _SmtkAppWin
{
	GtkApplicationWindow parent_instance;
};

G_DEFINE_TYPE(SmtkAppWin, smtk_app_win, GTK_TYPE_APPLICATION_WINDOW)

static void smtk_app_win_init(SmtkAppWin *win)
{
}

static void smtk_app_win_class_init(SmtkAppWinClass *win_class)
{
}

SmtkAppWin *smtk_app_win_new(SmtkApp *app)
{
	return g_object_new(SMTK_TYPE_APP_WIN, "application", app, NULL);
}
