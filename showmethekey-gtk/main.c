#include <gtk/gtk.h>

#include "smtk-app.h"

int main(int argc, char *argv[])
{
	SmtkApp *app = smtk_app_new();

	int result = g_application_run(G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return result;
}
