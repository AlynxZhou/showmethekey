#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "smtk-usage-win.h"

struct _SmtkUsageWin {
	AdwWindow parent_instance;
};
G_DEFINE_TYPE(SmtkUsageWin, smtk_usage_win, ADW_TYPE_WINDOW)

static void smtk_usage_win_class_init(SmtkUsageWinClass *klass)
{
	gtk_widget_class_set_template_from_resource(
		GTK_WIDGET_CLASS(klass),
		"/one/alynx/showmethekey/smtk-usage-win.ui"
	);
}

static void smtk_usage_win_init(SmtkUsageWin *this)
{
	gtk_widget_init_template(GTK_WIDGET(this));
}

GtkWidget *smtk_usage_win_new(void)
{
	return g_object_new(SMTK_TYPE_USAGE_WIN, NULL);
}
