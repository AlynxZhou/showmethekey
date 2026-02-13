#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "smtk-usage-win.h"

struct _SmtkUsageWin {
	AdwWindow parent_instance;
};
G_DEFINE_TYPE(SmtkUsageWin, smtk_usage_win, ADW_TYPE_WINDOW)

static void smtk_usage_win_init(SmtkUsageWin *win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void smtk_usage_win_dispose(GObject *object)
{
	G_OBJECT_CLASS(smtk_usage_win_parent_class)->dispose(object);
}

static void smtk_usage_win_class_init(SmtkUsageWinClass *win_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(win_class);

	object_class->dispose = smtk_usage_win_dispose;

	gtk_widget_class_set_template_from_resource(
		GTK_WIDGET_CLASS(win_class),
		"/one/alynx/showmethekey/smtk-usage-win.ui");
}

GtkWidget *smtk_usage_win_new(void)
{
	return g_object_new(SMTK_TYPE_USAGE_WIN, NULL);
}
