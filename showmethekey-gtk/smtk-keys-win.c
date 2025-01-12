#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#	include <gdk/x11/gdkx.h>
#endif

#include "smtk.h"
#include "smtk-keys-win.h"

struct _SmtkKeysWin {
	AdwWindow parent_instance;
	SmtkAppWin *app_win;
	GtkWidget *box;
	GtkWidget *header_bar;
	GtkWidget *handle;
	GtkWidget *area;
	SmtkKeysEmitter *emitter;
	SmtkKeyMode mode;
	SmtkKeyAlignment alignment;
	bool clickable;
	bool paused;
	bool show_shift;
	bool show_keyboard;
	bool show_mouse;
	bool draw_border;
	bool hide_visible;
	int timeout;
	char *layout;
	char *variant;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, ADW_TYPE_WINDOW)

enum { SIG_PAUSE, N_SIGNALS };

static unsigned int obj_signals[N_SIGNALS] = { 0 };

enum {
	PROP_0,
	PROP_CLICKABLE,
	PROP_SHOW_SHIFT,
	PROP_SHOW_KEYBOARD,
	PROP_SHOW_MOUSE,
	PROP_DRAW_BORDER,
	PROP_HIDE_VISIBLE,
	PROP_MODE,
	PROP_ALIGNMENT,
	PROP_TIMEOUT,
	PROP_LAYOUT,
	PROP_VARIANT,
	N_PROPS
};

static GParamSpec *obj_props[N_PROPS] = { NULL };

static void smtk_keys_win_set_property(GObject *object,
				       unsigned int property_id,
				       const GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_CLICKABLE:
		smtk_keys_win_set_clickable(win, g_value_get_boolean(value));
		break;
	case PROP_SHOW_SHIFT:
		smtk_keys_win_set_show_shift(win, g_value_get_boolean(value));
		break;
	case PROP_SHOW_KEYBOARD:
		smtk_keys_win_set_show_keyboard(win,
						g_value_get_boolean(value));
		break;
	case PROP_SHOW_MOUSE:
		smtk_keys_win_set_show_mouse(win, g_value_get_boolean(value));
		break;
	case PROP_DRAW_BORDER:
		smtk_keys_win_set_draw_border(win, g_value_get_boolean(value));
		break;
	case PROP_HIDE_VISIBLE:
		smtk_keys_win_set_hide_visible(win, g_value_get_boolean(value));
		break;
	case PROP_MODE:
		smtk_keys_win_set_mode(win, g_value_get_enum(value));
		break;
	case PROP_ALIGNMENT:
		smtk_keys_win_set_alignment(win, g_value_get_enum(value));
		break;
	case PROP_TIMEOUT:
		smtk_keys_win_set_timeout(win, g_value_get_int(value));
		break;
	case PROP_LAYOUT:
		smtk_keys_win_set_layout(win, g_value_get_string(value));
		break;
	case PROP_VARIANT:
		smtk_keys_win_set_variant(win, g_value_get_string(value));
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
	case PROP_SHOW_SHIFT:
		g_value_set_boolean(value, win->show_shift);
		break;
	case PROP_SHOW_KEYBOARD:
		g_value_set_boolean(value, win->show_keyboard);
		break;
	case PROP_SHOW_MOUSE:
		g_value_set_boolean(value, win->show_mouse);
		break;
	case PROP_DRAW_BORDER:
		g_value_set_boolean(value, win->draw_border);
		break;
	case PROP_HIDE_VISIBLE:
		g_value_set_boolean(value, win->hide_visible);
		break;
	case PROP_MODE:
		g_value_set_enum(value, win->mode);
		break;
	case PROP_ALIGNMENT:
		g_value_set_enum(value, win->alignment);
		break;
	case PROP_TIMEOUT:
		g_value_set_int(value, win->timeout);
		break;
	case PROP_LAYOUT:
		g_value_set_string(value, smtk_keys_win_get_layout(win));
		break;
	case PROP_VARIANT:
		g_value_set_string(value, smtk_keys_win_get_variant(win));
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

static void smtk_keys_win_emitter_on_pause(SmtkKeysWin *win)
{
	g_signal_emit_by_name(win, "pause");
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

	g_debug("Allocated size: %d×%d.", width, height);

	smtk_app_win_set_size(win->app_win, width, height);

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
	adw_window_set_content(ADW_WINDOW(win), win->box);

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

	win->emitter = smtk_keys_emitter_new(win->show_shift,
					     win->show_keyboard,
					     win->show_mouse, win->hide_visible,
					     win->mode, win->layout,
					     win->variant, &win->error);
	// `win->error` is set so just return.
	if (win->emitter == NULL)
		goto out;
	g_signal_connect_swapped(
		win->emitter, "error-cli-exit",
		G_CALLBACK(smtk_keys_win_emitter_on_error_cli_exit), win);
	g_signal_connect_swapped(win->emitter, "key",
				 G_CALLBACK(smtk_keys_win_emitter_on_key), win);
	g_signal_connect_swapped(win->emitter, "pause",
				 G_CALLBACK(smtk_keys_win_emitter_on_pause),
				 win);

	smtk_keys_emitter_start_async(win->emitter, &win->error);
	if (win->error != NULL)
		goto out;

	win->area = smtk_keys_area_new(win->mode, win->alignment,
				       win->draw_border, win->timeout);
	gtk_box_append(GTK_BOX(win->box), win->area);

out:
	G_OBJECT_CLASS(smtk_keys_win_parent_class)->constructed(object);
}

static void smtk_keys_win_dispose(GObject *object)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

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

	obj_signals[SIG_PAUSE] = g_signal_new("pause", SMTK_TYPE_KEYS_WIN,
					      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					      g_cclosure_marshal_VOID__VOID,
					      G_TYPE_NONE, 0);

	obj_props[PROP_CLICKABLE] = g_param_spec_boolean(
		"clickable", "Clickable", "Clickable or Click Through", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_MODE] = g_param_spec_enum(
		"mode", "Mode", "Key Mode", SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_ALIGNMENT] = g_param_spec_enum(
		"alignment", "Alignment", "Key Alignment",
		SMTK_TYPE_KEY_ALIGNMENT, SMTK_KEY_ALIGNMENT_END,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_SHOW_SHIFT] = g_param_spec_boolean(
		"show-shift", "Show Shift", "Show Shift Separately", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_SHOW_KEYBOARD] = g_param_spec_boolean(
		"show-keyboard", "Show Keyboard", "Show keyboard key", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_DRAW_BORDER] = g_param_spec_boolean(
		"draw-border", "Draw Border", "Draw Keys Border", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_HIDE_VISIBLE] = g_param_spec_boolean(
		"hide-visible", "Hide Visible", "Hide Visible Keys", false,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_LAYOUT] =
		g_param_spec_string("layout", "Layout", "Keymap Layout", NULL,
				    G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_VARIANT] = g_param_spec_string(
		"variant", "Variant", "Keymap Variant", NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPS, obj_props);
}

GtkWidget *smtk_keys_win_new(SmtkAppWin *app_win, bool clickable,
			     bool show_shift, bool show_keyboard,
			     bool show_mouse, bool draw_border,
			     bool hide_visible, SmtkKeyMode mode,
			     SmtkKeyAlignment alignment, int width, int height,
			     int timeout, const char *layout,
			     const char *variant, GError **error)
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
		"clickable", clickable, "mode", mode, "alignment", alignment,
		"show-shift", show_shift, "show-keyboard", show_keyboard,
		"show-mouse", show_mouse, "draw-border", draw_border,
		"hide-visible", hide_visible, "timeout", timeout, "layout",
		layout, "variant", variant, NULL);

	if (win->error != NULL) {
		g_propagate_error(error, win->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(win);
		gtk_window_destroy(GTK_WINDOW(win));
		return NULL;
	}

	win->app_win = app_win;

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	// Setting transient will block showing on all desktop so don't use it.
	// gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));

	// GTK always return GtkWidget, so do I.
	return GTK_WIDGET(win);
}

void smtk_keys_win_set_clickable(SmtkKeysWin *win, bool clickable)
{
	g_return_if_fail(win != NULL);

	// We don't need the handle if click through. But the handle might not
	// be there during init.
	if (win->handle != NULL)
		gtk_widget_set_visible(win->handle, clickable);

	// NOTE: We don't handle input region here, I don't know why we can't.
	// We just save property and handle the input region in
	// `size_allocate()`.
	// Sync self property.
	win->clickable = clickable;
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
	if (win->emitter != NULL)
		smtk_keys_emitter_set_show_shift(win->emitter, show_shift);
	// Sync self property.
	win->show_shift = show_shift;
}

void smtk_keys_win_set_show_keyboard(SmtkKeysWin *win, bool show_keyboard)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	if (win->emitter != NULL)
		smtk_keys_emitter_set_show_keyboard(win->emitter,
						    show_keyboard);
	// Sync self property.
	win->show_keyboard = show_keyboard;
}

void smtk_keys_win_set_show_mouse(SmtkKeysWin *win, bool show_mouse)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	if (win->emitter != NULL)
		smtk_keys_emitter_set_show_mouse(win->emitter, show_mouse);
	// Sync self property.
	win->show_mouse = show_mouse;
}

void smtk_keys_win_set_draw_border(SmtkKeysWin *win, bool draw_border)
{
	g_return_if_fail(win != NULL);

	// Pass property to area.
	if (win->area != NULL)
		smtk_keys_area_set_draw_border(SMTK_KEYS_AREA(win->area),
					       draw_border);
	// Sync self property.
	win->draw_border = draw_border;
}

void smtk_keys_win_set_hide_visible(SmtkKeysWin *win, bool hide_visible)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	if (win->emitter != NULL)
		smtk_keys_emitter_set_hide_visible(win->emitter, hide_visible);
	// Sync self property.
	win->hide_visible = hide_visible;
}

void smtk_keys_win_set_mode(SmtkKeysWin *win, SmtkKeyMode mode)
{
	g_return_if_fail(win != NULL);

	// Pass property to emitter.
	if (win->emitter != NULL)
		smtk_keys_emitter_set_mode(win->emitter, mode);
	// Pass property to area.
	if (win->area != NULL)
		smtk_keys_area_set_mode(SMTK_KEYS_AREA(win->area), mode);
	// Sync self property.
	win->mode = mode;
}

void smtk_keys_win_set_alignment(SmtkKeysWin *win, SmtkKeyAlignment alignment)
{
	g_return_if_fail(win != NULL);

	// Pass property to area.
	if (win->area != NULL)
		smtk_keys_area_set_alignment(SMTK_KEYS_AREA(win->area),
					     alignment);
	// Sync self property.
	win->alignment = alignment;
}

void smtk_keys_win_set_timeout(SmtkKeysWin *win, int timeout)
{
	g_return_if_fail(win != NULL);

	// Pass property to area.
	if (win->area != NULL)
		smtk_keys_area_set_timeout(SMTK_KEYS_AREA(win->area), timeout);
	// Sync self property.
	win->timeout = timeout;
}

const char *smtk_keys_win_get_layout(SmtkKeysWin *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->layout;
}

void smtk_keys_win_set_layout(SmtkKeysWin *win, const char *layout)
{
	g_return_if_fail(win != NULL);

	if (win->emitter != NULL)
		smtk_keys_emitter_set_layout(win->emitter, layout);

	if (win->layout != NULL)
		g_free(win->layout);
	win->layout = g_strdup(layout);
}

const char *smtk_keys_win_get_variant(SmtkKeysWin *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->variant;
}

void smtk_keys_win_set_variant(SmtkKeysWin *win, const char *variant)
{
	g_return_if_fail(win != NULL);

	if (win->emitter != NULL)
		smtk_keys_emitter_set_variant(win->emitter, variant);

	if (win->variant != NULL)
		g_free(win->variant);
	win->variant = g_strdup(variant);
}
