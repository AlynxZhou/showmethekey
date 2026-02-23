#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "config.h"
#include "smtk-app.h"

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	// Typically "/usr" "/" "share/locale".
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	g_autoptr(SmtkApp) app = smtk_app_new();

	return g_application_run(G_APPLICATION(app), argc, argv);
}
