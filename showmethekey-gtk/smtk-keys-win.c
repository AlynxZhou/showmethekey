#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#	include <gdk/x11/gdkx.h>
#endif

#include "smtk-keys-win.h"
#include "smtk-keys-area.h"
#include "smtk-keys-emitter.h"

struct _SmtkKeysWin {
	AdwApplicationWindow parent_instance;
	GSettings *settings;
	GtkWidget *box;
	GtkWidget *header_bar;
	GtkWidget *handle;
	GtkWidget *area;
	SmtkKeysEmitter *emitter;
	bool clickable;
	bool paused;
	bool show_shift;
	bool show_keyboard;
	bool show_mouse;
	bool hide_visible;
	char *layout;
	char *variant;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, ADW_TYPE_APPLICATION_WINDOW)

enum { PROP_0, PROP_CLICKABLE, PROP_PAUSED, N_PROPS };

static GParamSpec *obj_props[N_PROPS] = { NULL };

static void update_title(SmtkKeysWin *win)
{
	// The handle might not be there during init.
	if (win->handle == NULL)
		return;

	if (win->clickable && win->paused) {
		adw_window_title_set_title(ADW_WINDOW_TITLE(win->handle),
					   _("Clickable Paused"));
		gtk_widget_set_visible(win->handle, true);
	} else if (win->clickable) {
		adw_window_title_set_title(ADW_WINDOW_TITLE(win->handle),
					   _("Clickable"));
		gtk_widget_set_visible(win->handle, true);
	} else if (win->paused) {
		adw_window_title_set_title(ADW_WINDOW_TITLE(win->handle),
					   _("Paused"));
		gtk_widget_set_visible(win->handle, true);
	} else {
		adw_window_title_set_title(ADW_WINDOW_TITLE(win->handle), "");
		gtk_widget_set_visible(win->handle, false);
	}
}

static void smtk_keys_win_set_property(GObject *object,
				       unsigned int property_id,
				       const GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_CLICKABLE:
		// NOTE: We don't handle input region here, I don't know why we
		// can't do that. We just save property and handle the input
		// region in `size_allocate()`.
		win->clickable = g_value_get_boolean(value);
		update_title(win);
		break;
	case PROP_PAUSED:
		win->paused = g_value_get_boolean(value);
		update_title(win);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_win_get_property(GObject *object,
				       unsigned int property_id, GValue *value,
				       GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_CLICKABLE:
		g_value_set_boolean(value, win->clickable);
		break;
	case PROP_PAUSED:
		g_value_set_boolean(value, win->paused);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_win_emitter_on_error_cli_exit(SmtkKeysWin *win,
						    SmtkKeysEmitter *emitter)
{
	gtk_window_destroy(GTK_WINDOW(win));
}

static void smtk_keys_win_emitter_on_key(SmtkKeysWin *win, char key[])
{
	if (win->paused)
		return;

	// It seems that GObject closure will free string argument.
	// See <http://garfileo.is-programmer.com/2011/3/25/gobject-signal-extra-1.25576.html>.
	// void (*callback)(void *instance, const gchar *arg1, void *data)
	smtk_keys_area_add_key(SMTK_KEYS_AREA(win->area), g_strdup(key));
}

#ifdef GDK_WINDOWING_X11
// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gdk/x11/gdksurface-x11.c#L2310-2337>.
static void gdk_x11_surface_wmspec_change_state(GdkSurface *surface, bool add,
						const char *state)
{
	GdkDisplay *display = gdk_surface_get_display(surface);
	Display *xdisplay = gdk_x11_display_get_xdisplay(display);
	XClientMessageEvent xclient;

#	define _NET_WM_STATE_REMOVE 0
#	define _NET_WM_STATE_ADD 1
#	define _NET_WM_STATE_TOGGLE 2

	memset(&xclient, 0, sizeof(xclient));
	xclient.type = ClientMessage;
	xclient.window = gdk_x11_surface_get_xid(surface);
	xclient.display = xdisplay;
	xclient.message_type =
		gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_STATE");
	xclient.format = 32;
	xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xclient.data.l[1] =
		gdk_x11_get_xatom_by_name_for_display(display, state);
	xclient.data.l[2] = None;
	// Source indication.
	xclient.data.l[3] = 1;
	xclient.data.l[4] = 0;

	XSendEvent(xdisplay, gdk_x11_display_get_xrootwindow(display), False,
		   SubstructureRedirectMask | SubstructureNotifyMask,
		   (XEvent *)&xclient);
}

// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4122-4168>.
static void gdk_x11_surface_wmspec_change_desktop(GdkSurface *surface,
						  long desktop)
{
	GdkDisplay *display = gdk_surface_get_display(surface);
	Display *xdisplay = gdk_x11_display_get_xdisplay(display);
	XClientMessageEvent xclient;

	memset(&xclient, 0, sizeof(xclient));
	xclient.type = ClientMessage;
	xclient.window = gdk_x11_surface_get_xid(surface);
	xclient.display = xdisplay;
	xclient.message_type = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_DESKTOP");
	xclient.format = 32;
	xclient.data.l[0] = desktop;
	// Source indication.
	xclient.data.l[1] = 0;
	xclient.data.l[2] = 0;
	xclient.data.l[3] = 0;
	xclient.data.l[4] = 0;

	XSendEvent(xdisplay, gdk_x11_display_get_xrootwindow(display), False,
		   SubstructureRedirectMask | SubstructureNotifyMask,
		   (XEvent *)&xclient);
}
#endif

static void smtk_keys_win_on_map(SmtkKeysWin *win, void *data)
{
	// GTK4 dropped those API, so we need to implement those by ourselves
	// via X11 WMSpec.
	// See <https://discourse.gnome.org/t/setting-x11-properties-in-gtk4/9985/3>.
	// gtk_window_set_keep_above(GTK_WINDOW(win), true);
	// gtk_window_stick(GTK_WINDOW(win));
#ifdef GDK_WINDOWING_X11
	GtkNative *native = gtk_widget_get_native(GTK_WIDGET(win));
	if (native != NULL) {
		GdkSurface *surface = gtk_native_get_surface(native);
		GdkDisplay *display = gdk_surface_get_display(surface);
		if (GDK_IS_X11_DISPLAY(display)) {
			// Always on top.
			// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4383-4407>.
			// Need to remove _NET_WM_STATE_BELOW first.
			gdk_x11_surface_wmspec_change_state(
				surface, false, "_NET_WM_STATE_BELOW");
			gdk_x11_surface_wmspec_change_state(
				surface, true, "_NET_WM_STATE_ABOVE");

			// Always on visible workspaces.
			// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4122-4168>.
			// _NET_WM_STATE_STICKY only means WM should keep the
			// window's position fixed, even when scrolling virtual
			// desktops.
			gdk_x11_surface_wmspec_change_state(
				surface, true, "_NET_WM_STATE_STICKY");
			// See <https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45703946940912>.
			// Setting desktop to 0xFFFFFFFF means shows on all
			// desktops.
			// See <https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45703946960064>.
			gdk_x11_surface_wmspec_change_desktop(surface,
							      0xFFFFFFFF);
		}
	}
#endif
}

// NOTE: Not sure why but we can only alter input region in this function,
// calling `gdk_surface_set_input_region()` in setter is invalid.
static void smtk_keys_win_size_allocate(GtkWidget *widget, int width,
					int height, int baseline)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(widget);

	// We handle our magic after GTK done its internal layout compute.
	// We only read but not adjust allocation so this is safe.
	GTK_WIDGET_CLASS(smtk_keys_win_parent_class)
		->size_allocate(widget, width, height, baseline);

	g_debug("Allocated size: %d×%d.", width, height);

	GtkNative *native = gtk_widget_get_native(widget);
	if (native != NULL) {
		GdkSurface *surface = gtk_native_get_surface(native);
		if (win->clickable) {
			// See <https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_surface>.
			// The initial value for an input region is infinite.
			// That means the whole surface will accept input. A
			// NULL wl_region causes the input region to be set to
			// infinite.
			gdk_surface_set_input_region(surface, NULL);
		} else {
			cairo_region_t *empty_region = cairo_region_create();
			gdk_surface_set_input_region(surface, empty_region);
			cairo_region_destroy(empty_region);
		}
	}
}

static void smtk_keys_win_init(SmtkKeysWin *win)
{
	// TODO: Are those comments still true for GTK4?
	// It seems a widget from `.ui` file is unable to set to transparent.
	// So we have to make UI from code.
	win->error = NULL;
	win->clickable = false;
	win->paused = false;

	win->settings = NULL;
	win->handle = NULL;
	win->emitter = NULL;
	win->area = NULL;
	win->layout = NULL;
	win->variant = NULL;

	// AdwApplication will automatically load `style.css` under resource
	// base path, so we don't need to load it manually, just add a class so
	// we change style of the keys window only.
	gtk_widget_add_css_class(GTK_WIDGET(win), "smtk-keys-win");

	// Since libadwaita v1.6, it starts to set minimal size of window to
	// 320x200, however 200 is too large for some users when using this keys
	// window, so we unset it.
	//
	// See <https://gitlab.gnome.org/GNOME/libadwaita/-/commit/7a705c7959d784fa6d40af202d5d06d06f1e2fa6>.
	gtk_widget_set_size_request(GTK_WIDGET(win), -1, -1);

	// Don't know why but realize does not work.
	g_signal_connect(GTK_WIDGET(win), "map",
			 G_CALLBACK(smtk_keys_win_on_map), NULL);
}

static void smtk_keys_win_constructed(GObject *object)
{
	// Seems we can only get constructor properties here.
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	win->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	adw_application_window_set_content(ADW_APPLICATION_WINDOW(win),
					   win->box);

	// Allow user to choose position by drag this.
	win->header_bar = adw_header_bar_new();
	adw_header_bar_set_show_start_title_buttons(
		ADW_HEADER_BAR(win->header_bar), false);
	adw_header_bar_set_show_end_title_buttons(
		ADW_HEADER_BAR(win->header_bar), false);
	win->handle = adw_window_title_new(_("Clickable"), NULL);
	adw_header_bar_set_title_widget(ADW_HEADER_BAR(win->header_bar),
					win->handle);
	gtk_box_append(GTK_BOX(win->box), win->header_bar);
	gtk_widget_set_visible(win->handle, win->clickable);

	win->emitter = smtk_keys_emitter_new(&win->error);
	// `win->error` is set so just return.
	if (win->emitter == NULL)
		goto out;
	g_signal_connect_swapped(
		win->emitter, "error-cli-exit",
		G_CALLBACK(smtk_keys_win_emitter_on_error_cli_exit), win);
	g_signal_connect_swapped(win->emitter, "key",
				 G_CALLBACK(smtk_keys_win_emitter_on_key), win);

	smtk_keys_emitter_start_async(win->emitter, &win->error);
	if (win->error != NULL)
		goto out;

	win->area = smtk_keys_area_new();
	gtk_box_append(GTK_BOX(win->box), win->area);

	win->settings = g_settings_new("one.alynx.showmethekey");
	// Sync settings with initial state.
	g_settings_set_boolean(win->settings, "clickable", win->clickable);
	g_settings_set_boolean(win->settings, "paused", win->paused);
	g_settings_bind(win->settings, "clickable", win, "clickable",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "paused", win, "paused",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "width", win, "default-width",
			G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(win->settings, "height", win, "default-height",
			G_SETTINGS_BIND_DEFAULT);

out:
	G_OBJECT_CLASS(smtk_keys_win_parent_class)->constructed(object);
}

static void smtk_keys_win_dispose(GObject *object)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	g_clear_object(&win->settings);
	if (win->emitter != NULL) {
		smtk_keys_emitter_stop_async(win->emitter);
		g_object_unref(win->emitter);
		win->emitter = NULL;
	}

	G_OBJECT_CLASS(smtk_keys_win_parent_class)->dispose(object);
}

static void smtk_keys_win_finalize(GObject *object)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	g_clear_pointer(&win->layout, g_free);
	g_clear_pointer(&win->variant, g_free);

	G_OBJECT_CLASS(smtk_keys_win_parent_class)->finalize(object);
}

static void smtk_keys_win_class_init(SmtkKeysWinClass *win_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(win_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(win_class);

	object_class->set_property = smtk_keys_win_set_property;
	object_class->get_property = smtk_keys_win_get_property;

	object_class->constructed = smtk_keys_win_constructed;

	object_class->dispose = smtk_keys_win_dispose;
	object_class->finalize = smtk_keys_win_finalize;

	// In GTK4 size allocate is not a signal but a virtual method, but I
	// really need it.
	widget_class->size_allocate = smtk_keys_win_size_allocate;

	obj_props[PROP_CLICKABLE] = g_param_spec_boolean(
		"clickable", "Clickable", "Clickable or Click Through", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_PAUSED] =
		g_param_spec_boolean("paused", "Paused", "Paused keys", true,
				     G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPS, obj_props);
}

GtkWidget *smtk_keys_win_new(SmtkApp *app, bool clickable, GError **error)
{
	SmtkKeysWin *win = g_object_new(
		// Don't translate floating window's title, maybe users have
		// window rules for it.
		SMTK_TYPE_KEYS_WIN, "application", app, "title",
		"Floating Window - Show Me The Key", "icon-name",
		"one.alynx.showmethekey", "can-focus", false, "focus-on-click",
		false, "vexpand", false, "vexpand-set", true, "hexpand", false,
		"hexpand-set", true, "focusable", false, "resizable", true,
		// Wayland does not support this, it's ok.
		// "skip-pager-hint", true, "skip-taskbar-hint", true,
		// Reset state on keys win start.
		"clickable", clickable, "paused", false, NULL);

	if (win->error != NULL) {
		g_propagate_error(error, win->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(win);
		gtk_window_destroy(GTK_WINDOW(win));
		return NULL;
	}

	// gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	// Setting transient will block showing on all desktop so don't use it.
	// gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));

	// GTK always return GtkWidget, so do I.
	return GTK_WIDGET(win);
}
