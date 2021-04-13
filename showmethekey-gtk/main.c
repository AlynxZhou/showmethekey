#include <gtk/gtk.h>
#include <locale.h>

#include "smtk.h"
#include "smtk-app.h"

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	// Typically "/usr" "/" "share/locale".
	bindtextdomain(GETTEXT_PACKAGE, INSTALL_PREFIX "/" PACKAGE_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	SmtkApp *app = smtk_app_new();

	int result = g_application_run(G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return result;
}
