#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include "config.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"

struct _SmtkApp {
	AdwApplication parent_instance;
	GSettings *settings;
	GtkWidget *app_win;
	GtkWidget *keys_win;
	bool keys_win_opt;
	bool app_win_opt;
	bool clickable_opt;
};
G_DEFINE_TYPE(SmtkApp, smtk_app, ADW_TYPE_APPLICATION)

static void
clickable_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "clickable";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void pause_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "paused";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void shift_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "show-shift";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void
keyboard_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "show-keyboard";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void mouse_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "show-mouse";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void
border_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "show-border";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void
hide_visible_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	const char *key = "hide-visible";
	const bool value = g_settings_get_boolean(this->settings, key);
	g_settings_set_boolean(this->settings, key, !value);
}

static void usage_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	if (this->app_win != NULL)
		smtk_app_win_show_usage(SMTK_APP_WIN(this->app_win));
}

static void about_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	if (this->app_win != NULL)
		smtk_app_win_show_about(SMTK_APP_WIN(this->app_win));
}

static void quit_action(GSimpleAction *action, GVariant *parameter, void *data)
{
	SmtkApp *this = data;

	smtk_app_quit(this);
}

static void on_app_win_destroy(SmtkApp *this, GtkWidget *app_win)
{
	this->app_win = NULL;
}

static void on_keys_win_destroy(SmtkApp *this, GtkWidget *keys_win)
{
	this->keys_win = NULL;
	g_settings_set_boolean(this->settings, "active", false);
}

static void show_keys_win(SmtkApp *this)
{
	if (this->keys_win == NULL) {
		g_autoptr(GError) error = NULL;
		this->keys_win = smtk_keys_win_new(this, this->clickable_opt);
		if (error != NULL) {
			g_warning(
				"Failed to create keys win: %s.", error->message
			);
			return;
		}
		g_settings_set_boolean(this->settings, "active", true);
		g_signal_connect_swapped(
			this->keys_win,
			"destroy",
			G_CALLBACK(on_keys_win_destroy),
			this
		);
		gtk_window_present(GTK_WINDOW(this->keys_win));
	}
}

static void show_app_win(SmtkApp *this)
{
	if (this->app_win == NULL) {
		// Sync settings with current state to prevent inconsistency that
		// keys win is not shown but keys win switch is on (like SIGINT).
		g_settings_set_boolean(
			this->settings, "active", this->keys_win != NULL
		);
		this->app_win = smtk_app_win_new(this);
		g_signal_connect_swapped(
			this->app_win,
			"destroy",
			G_CALLBACK(on_app_win_destroy),
			this
		);
		gtk_window_present(GTK_WINDOW(this->app_win));
	}
}

static void on_active_changed(SmtkApp *this, char *key, GSettings *settings)
{
	if (g_settings_get_boolean(this->settings, "active"))
		show_keys_win(this);
	else if (this->keys_win != NULL)
		gtk_window_destroy(GTK_WINDOW(this->keys_win));
}

static void activate(GApplication *app)
{
	// Application is already single instance, and we use this to prevent
	// mutliply windows.
	SmtkApp *this = SMTK_APP(app);

	g_debug("Keys win: %s.", this->keys_win_opt ? "true" : "false");
	g_debug("App win: %s.", this->app_win_opt ? "true" : "false");
	g_debug("Clickable: %s.", this->clickable_opt ? "true" : "false");

	if (this->keys_win_opt)
		show_keys_win(this);
	if (this->app_win_opt)
		show_app_win(this);
}

static void startup(GApplication *app)
{
	SmtkApp *this = SMTK_APP(app);

	// Because application is not a construct property of GtkWindow,
	// we have to setup accels here.
	const GActionEntry actions[] = {
		{ "clickable", clickable_action, NULL, NULL, NULL },
		{ "pause", pause_action, NULL, NULL, NULL },
		{ "shift", shift_action, NULL, NULL, NULL },
		{ "keyboard", keyboard_action, NULL, NULL, NULL },
		{ "mouse", mouse_action, NULL, NULL, NULL },
		{ "border", border_action, NULL, NULL, NULL },
		{ "hide-visible", hide_visible_action, NULL, NULL, NULL },
		{ "usage", usage_action, NULL, NULL, NULL },
		{ "about", about_action, NULL, NULL, NULL },
		{ "quit", quit_action, NULL, NULL, NULL }
	};
	g_action_map_add_action_entries(
		G_ACTION_MAP(this), actions, G_N_ELEMENTS(actions), this
	);
	const char *clickable_accels[] = { "<Ctrl>C", NULL };
	const char *pause_accels[] = { "<Ctrl>P", NULL };
	const char *shift_accels[] = { "<Ctrl>S", NULL };
	const char *keyboard_accels[] = { "<Ctrl>K", NULL };
	const char *mouse_accels[] = { "<Ctrl>M", NULL };
	const char *border_accels[] = { "<Ctrl>B", NULL };
	const char *hide_visible_accels[] = { "<Ctrl>V", NULL };
	const char *usage_accels[] = { "<Ctrl>U", NULL };
	const char *about_accels[] = { "<Ctrl>A", NULL };
	const char *quit_accels[] = { "<Ctrl>Q", NULL };
	// See Description of
	// <https://developer.gnome.org/gio/stable/GActionMap.html>
	// about "app." here.
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.clickable", clickable_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.pause", pause_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.shift", shift_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.keyboard", keyboard_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.mouse", mouse_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.hide-visible", hide_visible_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.border", border_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.usage", usage_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.about", about_accels
	);
	gtk_application_set_accels_for_action(
		GTK_APPLICATION(this), "app.quit", quit_accels
	);

	G_APPLICATION_CLASS(smtk_app_parent_class)->startup(app);
}

// See <https://developer.gnome.org/gio/stable/GApplication.html#GApplication-handle-local-options>.
static int handle_local_options(GApplication *app, GVariantDict *options)
{
	SmtkApp *this = SMTK_APP(app);

	if (g_variant_dict_contains(options, "version")) {
		g_print(PROJECT_VERSION "\n");
		return 0;
	}

	if (g_variant_dict_contains(options, "active")) {
		g_warning("`-a, --active` is deprecated by `-k, --keys-win`.");
		this->keys_win_opt = true;
	}
	if (g_variant_dict_contains(options, "keys-win"))
		this->keys_win_opt = true;
	if (g_variant_dict_contains(options, "no-app-win")) {
		this->app_win_opt = false;
		// No one wants to run it with no window, hiding app window
		// means they want keys window directly.
		this->keys_win_opt = true;
	}
	if (g_variant_dict_contains(options, "no-clickable"))
		this->clickable_opt = false;

	return -1;
}

static void constructed(GObject *o)
{
	SmtkApp *this = SMTK_APP(o);

	this->settings = g_settings_new("one.alynx.showmethekey");
	g_signal_connect_swapped(
		this->settings,
		"changed::active",
		G_CALLBACK(on_active_changed),
		this
	);

	G_OBJECT_CLASS(smtk_app_parent_class)->constructed(o);
}

static void dispose(GObject *o)
{
	SmtkApp *this = SMTK_APP(o);

	g_clear_object(&this->settings);

	G_OBJECT_CLASS(smtk_app_parent_class)->dispose(o);
}

static void smtk_app_class_init(SmtkAppClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);
	GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

	o_class->constructed = constructed;
	o_class->dispose = dispose;

	app_class->activate = activate;
	app_class->startup = startup;

	// See <https://developer.gnome.org/CommandLine/>.
	app_class->handle_local_options = handle_local_options;
}

static void smtk_app_init(SmtkApp *this)
{
	this->settings = NULL;
	this->keys_win = NULL;
	this->app_win = NULL;
	this->keys_win_opt = false;
	this->app_win_opt = true;
	this->clickable_opt = true;

	g_set_application_name(_("Show Me The Key"));
	gtk_window_set_default_icon_name("one.alynx.showmethekey");

	const GOptionEntry options[] = {
		{ "version",
		  'v',
		  0,
		  G_OPTION_ARG_NONE,
		  NULL,
		  N_("Display version then exit."),
		  NULL },
		// Keep this option because I don't want to break CLI.
		{ "active",
		  'a',
		  0,
		  G_OPTION_ARG_NONE,
		  NULL,
		  N_(
			  "Show keys window on start up (deprecated by `-k, --keys-win`)."
		  ),
		  NULL },
		{ "keys-win",
		  'k',
		  0,
		  G_OPTION_ARG_NONE,
		  NULL,
		  N_("Show keys window on start up."),
		  NULL },
		{ "no-app-win",
		  'A',
		  0,
		  G_OPTION_ARG_NONE,
		  NULL,
		  N_("Hide app window and show keys window on start up."),
		  NULL },
		{ "no-clickable",
		  'C',
		  0,
		  G_OPTION_ARG_NONE,
		  NULL,
		  N_("Make keys window unclickable on start up."),
		  NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	g_application_add_main_option_entries(G_APPLICATION(this), options);
}

SmtkApp *smtk_app_new(void)
{
	return g_object_new(
		SMTK_TYPE_APP, "application-id", "one.alynx.showmethekey", NULL
	);
}

void smtk_app_quit(SmtkApp *this)
{
	g_return_if_fail(this != NULL);

	if (this->app_win != NULL) {
		gtk_window_destroy(GTK_WINDOW(this->app_win));
		this->app_win = NULL;
	}

	if (this->keys_win != NULL) {
		gtk_window_destroy(GTK_WINDOW(this->keys_win));
		this->keys_win = NULL;
	}

	g_application_quit(G_APPLICATION(this));
}
