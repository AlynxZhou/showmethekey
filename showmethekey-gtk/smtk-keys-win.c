#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#	include <gdk/x11/gdkx.h>
#endif

#include "smtk.h"
#include "smtk-keys-win.h"
#include "smtk-keys-area.h"
#include "smtk-keys-emitter.h"

struct _SmtkKeysWin {
	GtkWindow parent_instance;
	GtkWidget *header_bar;
	GtkWidget *handle;
	GtkWidget *area;
	GtkCssProvider *basic_css_provider;
	GtkCssProvider *window_css_provider;
	GtkCssProvider *header_bar_css_provider;
	SmtkKeysEmitter *emitter;
	SmtkKeyMode mode;
	bool show_shift;
	bool show_mouse;
	bool clickable;
	int timeout;
	bool paused;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

enum {
	PROP_0,
	PROP_MODE,
	PROP_SHOW_SHIFT,
	PROP_SHOW_MOUSE,
	PROP_CLICKABLE,
	PROP_TIMEOUT,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static void smtk_keys_win_set_property(GObject *object,
				       unsigned int property_id,
				       const GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_MODE:
		win->mode = g_value_get_enum(value);
		break;
	case PROP_SHOW_SHIFT:
		win->show_shift = g_value_get_boolean(value);
		break;
	case PROP_SHOW_MOUSE:
		win->show_mouse = g_value_get_boolean(value);
		break;
	case PROP_CLICKABLE:
		win->clickable = g_value_get_boolean(value);
		break;
	case PROP_TIMEOUT:
		win->timeout = g_value_get_int(value);
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
	case PROP_MODE:
		g_value_set_enum(value, win->mode);
		break;
	case PROP_SHOW_SHIFT:
		g_value_set_boolean(value, win->show_shift);
		break;
	case PROP_SHOW_MOUSE:
		g_value_set_boolean(value, win->show_mouse);
		break;
	case PROP_CLICKABLE:
		g_value_set_boolean(value, win->clickable);
		break;
	case PROP_TIMEOUT:
		g_value_set_int(value, win->timeout);
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
	// void (*callback)(gpointer instance, const gchar *arg1, gpointer user_data)
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

static void smtk_keys_win_on_map(SmtkKeysWin *win, gpointer user_data)
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

	g_debug("Allocated size: %dx%d.", width, height);

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
	win->paused = false;

	// Allow user to choose position by drag this.
	win->header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(win->header_bar),
					      false);
	// Disable subtitle to get a compact header bar.
	// gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(win->header_bar), false);
	win->handle = gtk_label_new(_("Clickable"));
	gtk_header_bar_set_title_widget(GTK_HEADER_BAR(win->header_bar),
					win->handle);
	gtk_window_set_titlebar(GTK_WINDOW(win), win->header_bar);

	// SmtkKeysWin is not a normal window, we use custom CSS and code to
	// make it a semi-transparent window, but some strange theme will break
	// our code (for example, Colloid), instead of writing workaround for
	// countless third-party themes, just using basic theme is reasonable.
	// We don't change GtkSettings:gtk-theme-name, because it will affect
	// all widgets of this program.
	win->basic_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_named(win->basic_css_provider, "gtk", "dark");

	// We don't want to paint the app shadow and decoration,
	// so just use a custom CSS to disable decoration outside the window.
	win->window_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(
		win->window_css_provider,
		"/one/alynx/showmethekey/smtk-keys-win.css");
	GtkStyleContext *window_style_context =
		gtk_widget_get_style_context(GTK_WIDGET(win));
	gtk_style_context_add_class(window_style_context, "smtk-keys-win");
	gtk_style_context_add_provider(
		window_style_context,
		GTK_STYLE_PROVIDER(win->window_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	// Override custom theme by setting provider in settings priority.
	// Only this priority works, it seems that I cannot set theme priority.
	gtk_style_context_add_provider(
		window_style_context,
		GTK_STYLE_PROVIDER(win->basic_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

	// It turns out that GtkStyleContext cannot affect child GtkWidgets,
	// so we have to create independent CSS for headerbar.
	win->header_bar_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(
		win->header_bar_css_provider,
		"/one/alynx/showmethekey/smtk-keys-win-header-bar.css");
	GtkStyleContext *header_bar_style_context =
		gtk_widget_get_style_context(win->header_bar);
	gtk_style_context_add_class(header_bar_style_context, "header-bar");
	gtk_style_context_add_provider(
		header_bar_style_context,
		GTK_STYLE_PROVIDER(win->header_bar_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	// Override custom theme by setting provider in settings priority.
	// Only this priority works, it seems that I cannot set theme priority.
	gtk_style_context_add_provider(
		header_bar_style_context,
		GTK_STYLE_PROVIDER(win->basic_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

	// Don't know why but realize does not work.
	g_signal_connect(GTK_WIDGET(win), "map",
			 G_CALLBACK(smtk_keys_win_on_map), NULL);
}

static void smtk_keys_win_constructed(GObject *object)
{
	// Seems we can only get constructor properties here.
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	win->emitter = smtk_keys_emitter_new(win->show_shift, win->show_mouse,
					     win->mode, &win->error);
	// `win->error` is set so just return.
	if (win->emitter == NULL)
		goto out;
	g_signal_connect_object(
		win->emitter, "error-cli-exit",
		G_CALLBACK(smtk_keys_win_emitter_on_error_cli_exit), win,
		G_CONNECT_SWAPPED);
	g_signal_connect_object(win->emitter, "key",
				G_CALLBACK(smtk_keys_win_emitter_on_key), win,
				G_CONNECT_SWAPPED);

	smtk_keys_emitter_start_async(win->emitter, &win->error);
	if (win->error != NULL)
		goto out;

	win->area = smtk_keys_area_new(win->timeout);
	gtk_window_set_child(GTK_WINDOW(win), win->area);

out:
	G_OBJECT_CLASS(smtk_keys_win_parent_class)->constructed(object);
}

static void smtk_keys_win_dispose(GObject *object)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	g_clear_object(&win->basic_css_provider);
	g_clear_object(&win->window_css_provider);
	g_clear_object(&win->header_bar_css_provider);

	if (win->emitter != NULL) {
		smtk_keys_emitter_stop_async(win->emitter);
		g_object_unref(win->emitter);
		win->emitter = NULL;
	}

	G_OBJECT_CLASS(smtk_keys_win_parent_class)->dispose(object);
}

static void smtk_keys_win_class_init(SmtkKeysWinClass *win_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(win_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(win_class);

	object_class->set_property = smtk_keys_win_set_property;
	object_class->get_property = smtk_keys_win_get_property;

	object_class->constructed = smtk_keys_win_constructed;

	object_class->dispose = smtk_keys_win_dispose;

	// In GTK4 size allocate is not a signal but a virtual method, but I
	// really need it.
	widget_class->size_allocate = smtk_keys_win_size_allocate;

	obj_properties[PROP_MODE] = g_param_spec_enum(
		"mode", "Mode", "Key Mode", SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_properties[PROP_SHOW_SHIFT] = g_param_spec_boolean(
		"show-shift", "Show Shift", "Show Shift Separately", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_properties[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_properties[PROP_CLICKABLE] = g_param_spec_boolean(
		"clickable", "Clickable", "Clickable or Click Through", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_properties[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

GtkWidget *smtk_keys_win_new(bool show_shift, bool show_mouse, SmtkKeyMode mode,
			     int width, int height, int timeout, GError **error)
{
	SmtkKeysWin *win = g_object_new(
		// Don't translate floating window's title, maybe users have
		// window rules for it.
		SMTK_TYPE_KEYS_WIN, "visible", true, "title",
		"Floating Window - Show Me The Key", "icon-name",
		"one.alynx.showmethekey", "can-focus", false, "focus-on-click",
		false, "vexpand", false, "vexpand-set", true, "hexpand", false,
		"hexpand-set", true, "focusable", false, "resizable", true,
		// Wayland does not support this, it's ok.
		// "skip-pager-hint", true, "skip-taskbar-hint", true,
		"mode", mode, "show-shift", show_shift, "show-mouse",
		show_mouse,
		// Window should not be click through by default.
		"clickable", true, "timeout", timeout, NULL);

	if (win->error != NULL) {
		g_propagate_error(error, win->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(win);
		gtk_window_destroy(GTK_WINDOW(win));
		return NULL;
	}

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	// Setting transient will block showing on all desktop so don't use it.
	// gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));

	// GTK always return GtkWidget, so do I.
	return GTK_WIDGET(win);
}

void smtk_keys_win_pause(SmtkKeysWin *win)
{
	g_return_if_fail(win != NULL);

	win->paused = true;
}

void smtk_keys_win_resume(SmtkKeysWin *win)
{
	g_return_if_fail(win != NULL);

	win->paused = false;
}

void smtk_keys_win_set_show_shift(SmtkKeysWin *win, bool show_shift)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	smtk_keys_emitter_set_show_shift(win->emitter, show_shift);
	// Sync self property.
	g_object_set(win, "show-shift", show_shift, NULL);
}

void smtk_keys_win_set_show_mouse(SmtkKeysWin *win, bool show_mouse)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	smtk_keys_emitter_set_show_mouse(win->emitter, show_mouse);
	// Sync self property.
	g_object_set(win, "show-mouse", show_mouse, NULL);
}

void smtk_keys_win_set_clickable(SmtkKeysWin *win, bool clickable)
{
	g_return_if_fail(win != NULL);

	// We don't need the handle if click through.
	gtk_widget_set_visible(win->handle, clickable);

	// NOTE: We don't handle input region here, I don't know why we can't.
	// We just save property and handle the input region in
	// `size_allocate()`.
	// Sync self property.
	g_object_set(win, "clickable", clickable, NULL);
}

void smtk_keys_win_set_mode(SmtkKeysWin *win, SmtkKeyMode mode)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	smtk_keys_emitter_set_mode(win->emitter, mode);
	// Sync self property.
	g_object_set(win, "mode", mode, NULL);
}

void smtk_keys_win_set_timeout(SmtkKeysWin *win, int timeout)
{
	g_return_if_fail(win != NULL);

	// Pass property to area.
	smtk_keys_area_set_timeout(SMTK_KEYS_AREA(win->area), timeout);
	// Sync self property.
	g_object_set(win, "timeout", timeout, NULL);
}
