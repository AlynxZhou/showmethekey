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
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, ADW_TYPE_APPLICATION_WINDOW)

enum { PROP_0, PROP_CLICKABLE, PROP_PAUSED, N_PROPS };

static GParamSpec *props[N_PROPS] = { NULL };

static void update_title(SmtkKeysWin *this)
{
	// The handle might not be there during init.
	if (this->handle == NULL)
		return;

	if (this->clickable && this->paused) {
		adw_window_title_set_title(
			ADW_WINDOW_TITLE(this->handle), _("Clickable Paused")
		);
		gtk_widget_set_visible(this->handle, true);
	} else if (this->clickable) {
		adw_window_title_set_title(
			ADW_WINDOW_TITLE(this->handle), _("Clickable")
		);
		gtk_widget_set_visible(this->handle, true);
	} else if (this->paused) {
		adw_window_title_set_title(
			ADW_WINDOW_TITLE(this->handle), _("Paused")
		);
		gtk_widget_set_visible(this->handle, true);
	} else {
		adw_window_title_set_title(ADW_WINDOW_TITLE(this->handle), "");
		gtk_widget_set_visible(this->handle, false);
	}
}

static void set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeysWin *this = SMTK_KEYS_WIN(o);

	switch (prop) {
	case PROP_CLICKABLE:
		// NOTE: We don't handle input region here, I don't know why we
		// can't do that. We just save property and handle the input
		// region in `size_allocate()`.
		this->clickable = g_value_get_boolean(value);
		update_title(this);
		break;
	case PROP_PAUSED:
		this->paused = g_value_get_boolean(value);
		update_title(this);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void
get_property(GObject *o, unsigned int prop, GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *this = SMTK_KEYS_WIN(o);

	switch (prop) {
	case PROP_CLICKABLE:
		g_value_set_boolean(value, this->clickable);
		break;
	case PROP_PAUSED:
		g_value_set_boolean(value, this->paused);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void on_error_cli_exit(SmtkKeysWin *this, SmtkKeysEmitter *emitter)
{
	gtk_window_destroy(GTK_WINDOW(this));
}

static void on_key(SmtkKeysWin *this, char key[])
{
	if (this->paused)
		return;

	// It seems that GObject closure will free string argument.
	//
	// See <http://garfileo.is-programmer.com/2011/3/25/gobject-signal-extra-1.25576.html>.
	// void (*callback)(void *instance, const gchar *arg1, void *data)
	smtk_keys_area_add_key(SMTK_KEYS_AREA(this->area), key);
}

#ifdef GDK_WINDOWING_X11
// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gdk/x11/gdksurface-x11.c#L2310-2337>.
static void gdk_x11_surface_wmspec_change_state(
	GdkSurface *surface,
	bool add,
	const char *state
)
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

	XSendEvent(
		xdisplay,
		gdk_x11_display_get_xrootwindow(display),
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		(XEvent *)&xclient
	);
}

// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4122-4168>.
static void
gdk_x11_surface_wmspec_change_desktop(GdkSurface *surface, long desktop)
{
	GdkDisplay *display = gdk_surface_get_display(surface);
	Display *xdisplay = gdk_x11_display_get_xdisplay(display);
	XClientMessageEvent xclient;

	memset(&xclient, 0, sizeof(xclient));
	xclient.type = ClientMessage;
	xclient.window = gdk_x11_surface_get_xid(surface);
	xclient.display = xdisplay;
	xclient.message_type = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_DESKTOP"
	);
	xclient.format = 32;
	xclient.data.l[0] = desktop;
	// Source indication.
	xclient.data.l[1] = 0;
	xclient.data.l[2] = 0;
	xclient.data.l[3] = 0;
	xclient.data.l[4] = 0;

	XSendEvent(
		xdisplay,
		gdk_x11_display_get_xrootwindow(display),
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		(XEvent *)&xclient
	);
}
#endif

static void on_map(SmtkKeysWin *this, void *data)
{
	// GTK4 dropped those API, so we need to implement those by ourselves
	// via X11 WMSpec.
	// See <https://discourse.gnome.org/t/setting-x11-properties-in-gtk4/9985/3>.
	// gtk_window_set_keep_above(GTK_WINDOW(win), true);
	// gtk_window_stick(GTK_WINDOW(win));
#ifdef GDK_WINDOWING_X11
	GtkNative *native = gtk_widget_get_native(GTK_WIDGET(this));
	if (native == NULL)
		return;

	GdkSurface *surface = gtk_native_get_surface(native);
	GdkDisplay *display = gdk_surface_get_display(surface);
	if (!GDK_IS_X11_DISPLAY(display))
		return;

	// Always on top.
	// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4383-4407>.
	//
	// Need to remove _NET_WM_STATE_BELOW first.
	gdk_x11_surface_wmspec_change_state(
		surface, false, "_NET_WM_STATE_BELOW"
	);
	gdk_x11_surface_wmspec_change_state(
		surface, true, "_NET_WM_STATE_ABOVE"
	);

	// Always on visible workspaces.
	// See <https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/x11/gdkwindow-x11.c#L4122-4168>.
	//
	// _NET_WM_STATE_STICKY only means WM should keep the window's position
	// fixed, even when scrolling virtual desktops.
	gdk_x11_surface_wmspec_change_state(
		surface, true, "_NET_WM_STATE_STICKY"
	);
	// See <https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45703946940912>.
	//
	// Setting desktop to 0xFFFFFFFF means shows on all desktops.
	//
	// See <https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45703946960064>.
	gdk_x11_surface_wmspec_change_desktop(surface, 0xFFFFFFFF);
#endif
}

// NOTE: Not sure why but we can only alter input region in this function,
// calling `gdk_surface_set_input_region()` in setter is invalid.
static void
size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
	SmtkKeysWin *this = SMTK_KEYS_WIN(widget);

	// We handle our magic after GTK done its internal layout compute.
	// We only read but not adjust allocation so this is safe.
	GTK_WIDGET_CLASS(smtk_keys_win_parent_class)
		->size_allocate(widget, width, height, baseline);

	g_debug("Allocated size: %d×%d.", width, height);

	GtkNative *native = gtk_widget_get_native(widget);
	if (native == NULL)
		return;

	GdkSurface *surface = gtk_native_get_surface(native);
	if (this->clickable) {
		// See <https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_surface>.
		// The initial value for an input region is infinite, that means
		// the whole surface will accept input. A NULL wl_region causes
		// the input region to be set to infinite.
		gdk_surface_set_input_region(surface, NULL);
	} else {
		cairo_region_t *empty_region = cairo_region_create();
		gdk_surface_set_input_region(surface, empty_region);
		cairo_region_destroy(empty_region);
	}
}

static void constructed(GObject *o)
{
	// Seems we can only get constructor properties here.
	SmtkKeysWin *this = SMTK_KEYS_WIN(o);

	// AdwApplication will automatically load `style.css` under resource
	// base path, so we don't need to load it manually, just add a class so
	// we change style of the keys window only.
	gtk_widget_add_css_class(GTK_WIDGET(this), "smtk-keys-win");

	// Since libadwaita v1.6, it starts to set minimal size of window to
	// 320x200, however 200 is too large for some users when using this keys
	// window, so we unset it.
	//
	// See <https://gitlab.gnome.org/GNOME/libadwaita/-/commit/7a705c7959d784fa6d40af202d5d06d06f1e2fa6>.
	gtk_widget_set_size_request(GTK_WIDGET(this), -1, -1);

	// Don't know why but realize does not work.
	g_signal_connect(this, "map", G_CALLBACK(on_map), NULL);

	this->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	adw_application_window_set_content(
		ADW_APPLICATION_WINDOW(this), this->box
	);

	// Allow user to choose position by drag this.
	this->header_bar = adw_header_bar_new();
	adw_header_bar_set_show_start_title_buttons(
		ADW_HEADER_BAR(this->header_bar), false
	);
	adw_header_bar_set_show_end_title_buttons(
		ADW_HEADER_BAR(this->header_bar), false
	);
	this->handle = adw_window_title_new(_("Clickable"), NULL);
	adw_header_bar_set_title_widget(
		ADW_HEADER_BAR(this->header_bar), this->handle
	);
	gtk_box_append(GTK_BOX(this->box), this->header_bar);
	gtk_widget_set_visible(this->handle, this->clickable);

	this->emitter = smtk_keys_emitter_new(&this->error);
	// `this->error` is set so just return.
	if (this->emitter == NULL)
		goto out;
	g_signal_connect_swapped(
		this->emitter,
		"error-cli-exit",
		G_CALLBACK(on_error_cli_exit),
		this
	);
	g_signal_connect_swapped(this->emitter, "key", G_CALLBACK(on_key), this);

	smtk_keys_emitter_start_async(this->emitter, &this->error);
	if (this->error != NULL)
		goto out;

	this->area = smtk_keys_area_new();
	gtk_box_append(GTK_BOX(this->box), this->area);

	this->settings = g_settings_new("one.alynx.showmethekey");
	// Sync settings with initial state.
	g_settings_set_boolean(this->settings, "clickable", this->clickable);
	g_settings_set_boolean(this->settings, "paused", this->paused);
	g_settings_bind(
		this->settings,
		"clickable",
		this,
		"clickable",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings, "paused", this, "paused", G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"width",
		this,
		"default-width",
		G_SETTINGS_BIND_DEFAULT
	);
	g_settings_bind(
		this->settings,
		"height",
		this,
		"default-height",
		G_SETTINGS_BIND_DEFAULT
	);

out:
	G_OBJECT_CLASS(smtk_keys_win_parent_class)->constructed(o);
}

static void dispose(GObject *o)
{
	SmtkKeysWin *this = SMTK_KEYS_WIN(o);

	g_clear_object(&this->settings);
	if (this->emitter != NULL) {
		smtk_keys_emitter_stop_async(this->emitter);
		g_object_unref(this->emitter);
		this->emitter = NULL;
	}

	G_OBJECT_CLASS(smtk_keys_win_parent_class)->dispose(o);
}

// static void finalize(GObject *o)
// {
// 	SmtkKeysWin *this = SMTK_KEYS_WIN(o);

// 	G_OBJECT_CLASS(smtk_keys_win_parent_class)->finalize(o);
// }

static void smtk_keys_win_class_init(SmtkKeysWinClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *w_class = GTK_WIDGET_CLASS(klass);

	o_class->set_property = set_property;
	o_class->get_property = get_property;

	o_class->constructed = constructed;

	o_class->dispose = dispose;
	// o_class->finalize = finalize;

	// In GTK4 size allocate is not a signal but a virtual method, but I
	// really need it.
	w_class->size_allocate = size_allocate;

	props[PROP_CLICKABLE] = g_param_spec_boolean(
		"clickable",
		"Clickable",
		"Clickable or Click Through",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_PAUSED] = g_param_spec_boolean(
		"paused",
		"Paused",
		"Paused keys",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_keys_win_init(SmtkKeysWin *this)
{
	// TODO: Are those comments still true for GTK4?
	// It seems a widget from `.ui` file is unable to set to transparent.
	// So we have to make UI from code.
	this->error = NULL;
	this->clickable = false;
	this->paused = false;

	this->settings = NULL;
	this->handle = NULL;
	this->emitter = NULL;
	this->area = NULL;
}

GtkWidget *smtk_keys_win_new(SmtkApp *app, bool clickable, GError **error)
{
	SmtkKeysWin *this = g_object_new(
		SMTK_TYPE_KEYS_WIN,
		"application",
		app,
		// Don't translate floating window's title, maybe users have
		// window rules for it.
		"title",
		"Floating Window - Show Me The Key",
		"icon-name",
		"one.alynx.showmethekey",
		"can-focus",
		false,
		"focus-on-click",
		false,
		"vexpand",
		false,
		"vexpand-set",
		true,
		"hexpand",
		false,
		"hexpand-set",
		true,
		"focusable",
		false,
		"resizable",
		true,
		// Wayland does not support this, it's ok.
		// "skip-pager-hint", true, "skip-taskbar-hint", true,
		// Reset state on keys win start.
		"clickable",
		clickable,
		"paused",
		false,
		NULL
	);

	if (this->error != NULL) {
		g_propagate_error(error, this->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(this);
		gtk_window_destroy(GTK_WINDOW(this));
		return NULL;
	}

	// gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	// Setting transient will block showing on all desktop so don't use it.
	// gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));

	// GTK always return GtkWidget, so do I.
	return GTK_WIDGET(this);
}
