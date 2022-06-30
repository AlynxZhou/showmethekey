#include "cairo.h"
#include "pango/pango-font.h"
#include "pango/pango-layout.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "smtk.h"
#include "smtk-keys-win.h"
#include "smtk-keys-emitter.h"

struct _SmtkKeysWin {
	GtkWindow parent_instance;
	GtkWidget *header_bar;
	GtkWidget *handle;
	GtkWidget *keys_label;
	PangoFontDescription *keys_font;
	SmtkKeysEmitter *emitter;
	SmtkKeyMode mode;
	bool show_mouse;
	int timeout;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

enum { PROP_0, PROP_MODE, PROP_SHOW_MOUSE, PROP_TIMEOUT, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static void smtk_keys_win_set_property(GObject *object, unsigned int property_id,
				       const GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_MODE:
		win->mode = g_value_get_enum(value);
		break;
	case PROP_SHOW_MOUSE:
		win->show_mouse = g_value_get_boolean(value);
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

static void smtk_keys_win_get_property(GObject *object, unsigned int property_id,
				       GValue *value, GParamSpec *pspec)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	switch (property_id) {
	case PROP_MODE:
		g_value_set_enum(value, win->mode);
		break;
	case PROP_SHOW_MOUSE:
		g_value_set_boolean(value, win->show_mouse);
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
	// It seems calling g_object_unref() on GtkWindow is not enough?
	// We need to call gtk_window_destroy().
	gtk_window_destroy(GTK_WINDOW(win));
}

static void smtk_keys_win_emitter_on_update_label(SmtkKeysWin *win,
						  char *label_text,
						  SmtkKeysEmitter *emitter)
{
	gtk_label_set_markup(GTK_LABEL(win->keys_label), label_text);
	// It seems that we cannot free argument in callback.
	// Looks like GValue will automatically free this for us.
	// And this is not the same as the outside one.
	// g_free(label_text);
}

static void smtk_keys_win_size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(widget);

	// We handle our magic after GTK done its internal layout compute.
	// We only read but not adjust allocation so this is safe.
	GTK_WIDGET_CLASS(smtk_keys_win_parent_class)->size_allocate(widget, width, height, baseline);

	g_debug("Size: %dx%d\n", width, height);

	// I am not sure why the avaliable height * PANGO_SCALE is too big,
	// just make it smaller, also too big will have less chars.
	pango_font_description_set_size(win->keys_font, height * 0.3 * PANGO_SCALE);
	// GtkLabel maybe re-create layout, so we have to update this everytime.
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(win->keys_label));
	pango_layout_set_font_description(layout, win->keys_font);

	// Widget's allocation is only usable after realize.
	GtkAllocation handle_allocation;
	gtk_widget_get_allocation(win->handle, &handle_allocation);
	g_debug("Clickable Area: x: %d, y: %d, w: %d, h: %d\n", handle_allocation.x, handle_allocation.y, handle_allocation.width, handle_allocation.height);
	cairo_region_t *clickable_region =
		cairo_region_create_rectangle(&handle_allocation);
	GtkNative *native = gtk_widget_get_native(widget);
	if (native != NULL) {
		GdkSurface *surface = gtk_native_get_surface(native);
		gdk_surface_set_input_region(surface, clickable_region);
	}
	cairo_region_destroy(clickable_region);
}

static void smtk_keys_win_init(SmtkKeysWin *win)
{
	// TODO: Is this still true for GTK4?
	// It seems a widget from `.ui` file is unable to set to transparent.
	// So we have to make UI from code.
	win->error = NULL;

	// Allow user to choose position by drag this.
	win->header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(win->header_bar), false);
	// Disable subtitle to get a compact header bar.
	// gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(win->header_bar), false);
	win->handle = gtk_label_new(_("Clickable Area"));
	gtk_header_bar_set_title_widget(GTK_HEADER_BAR(win->header_bar),
					win->handle);
	gtk_window_set_titlebar(GTK_WINDOW(win), win->header_bar);

	// TODO: Maybe it's better to use Cairo/Pango/GtkBox to do some custom
	// widget drawing, it's easier to fit container and looks better.
	// CSS is used to make a transparent titlebar,
	// because it's above window.
	win->keys_label = gtk_label_new(NULL);
	// If there are too much keys, we hide older keys.
	gtk_label_set_ellipsize(GTK_LABEL(win->keys_label),
				PANGO_ELLIPSIZE_START);
	// Center is better for user.
	gtk_label_set_justify(GTK_LABEL(win->keys_label), GTK_JUSTIFY_CENTER);
	// This prevent label from resizing window. But it is confusing,
	// it does not limit the label to only 1 char, which is I want.
	// See <https://developer.gnome.org/gtk3/stable/GtkLabel.html#label-text-layout>.
	gtk_label_set_max_width_chars(GTK_LABEL(win->keys_label), 1);
	gtk_label_set_use_markup(GTK_LABEL(win->keys_label), true);
	// By default label's font description is NULL which means inherited.
	// We give it a special one.
	win->keys_font = pango_font_description_new();
	pango_font_description_set_family(win->keys_font, "monospace");
	gtk_window_set_child(GTK_WINDOW(win), win->keys_label);

	// GTK4 dropped those API, so we need to implement
	// those when upgrading GTK4 by ourselves via WM hints.
	// See <https://discourse.gnome.org/t/setting-x11-properties-in-gtk4/9985/3>.
	// gtk_window_set_keep_above(GTK_WINDOW(win), true);
	// gtk_window_stick(GTK_WINDOW(win));

	// We don't want to paint the app shadow and decoration,
	// so just use a custom CSS to disable decoration outside the window.
	GtkCssProvider *window_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(
		window_css_provider,
		"/one/alynx/showmethekey/smtk-keys-win.css");
	GtkStyleContext *window_style_context =
		gtk_widget_get_style_context(GTK_WIDGET(win));
	gtk_style_context_add_class(window_style_context, "smtk-keys-win");
	gtk_style_context_add_provider(window_style_context,
				       GTK_STYLE_PROVIDER(window_css_provider),
				       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	// It turns out that GtkStyleContext cannot affect child GtkWidgets,
	// so we have to create independent CSS for headerbar.
	GtkCssProvider *header_bar_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(
		header_bar_css_provider,
		"/one/alynx/showmethekey/smtk-keys-win-header-bar.css");
	GtkStyleContext *header_bar_style_context =
		gtk_widget_get_style_context(win->header_bar);
	gtk_style_context_add_provider(
		header_bar_style_context,
		GTK_STYLE_PROVIDER(header_bar_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	// It turns out that GtkStyleContext cannot affect child GtkWidgets,
	// so we have to create independent CSS for keys label.
	GtkCssProvider *keys_label_css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(
		keys_label_css_provider,
		"/one/alynx/showmethekey/smtk-keys-win-keys-label.css");
	GtkStyleContext *keys_label_style_context =
		gtk_widget_get_style_context(win->keys_label);
	gtk_style_context_add_provider(
		keys_label_style_context,
		GTK_STYLE_PROVIDER(keys_label_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void smtk_keys_win_constructed(GObject *object)
{
	// Seems we can only get constructor properties here.
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	win->emitter = smtk_keys_emitter_new(win->show_mouse, win->mode,
					     win->timeout, &win->error);
	// win->error is set so just return.
	if (win->emitter == NULL)
		return;
	g_signal_connect_object(
		win->emitter, "error-cli-exit",
		G_CALLBACK(smtk_keys_win_emitter_on_error_cli_exit), win,
		G_CONNECT_SWAPPED);
	g_signal_connect_object(
		win->emitter, "update-label",
		G_CALLBACK(smtk_keys_win_emitter_on_update_label), win,
		G_CONNECT_SWAPPED);

	smtk_keys_emitter_start_async(win->emitter, &win->error);
	if (win->error != NULL)
		return;

	G_OBJECT_CLASS(smtk_keys_win_parent_class)->constructed(object);
}

static void smtk_keys_win_dispose(GObject *object)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(object);

	g_clear_pointer(&win->keys_font, pango_font_description_free);

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

	obj_properties[PROP_MODE] =
		g_param_spec_enum("mode", "Mode", "Key Mode",
				  SMTK_TYPE_KEY_MODE, SMTK_KEY_MODE_COMPOSED,
				  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	obj_properties[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", true,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	obj_properties[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

GtkWidget *smtk_keys_win_new(SmtkAppWin *parent, bool show_mouse, SmtkKeyMode mode,
			     int width, int height, int timeout,
			     GError **error)
{
	SmtkKeysWin *win = g_object_new(
		// Don't translate floating window's title, maybe users have
		// window rules for it.
		SMTK_TYPE_KEYS_WIN, "visible", true, "title",
		"Floating Window - Show Me The Key", "icon-name",
		"one.alynx.showmethekey", "can-focus", false, "focus-on-click",
		false,
		"vexpand", false, "vexpand-set", true,
		"hexpand", false, "hexpand-set", true,
		// We cannot focus on this window, and it has no border,
		// so user resize is meaningless for it.
		"resizable", false,
		// Wayland does not support this, it's ok.
		// "skip-pager-hint", true, "skip-taskbar-hint", true,
		"mode", mode, "show-mouse", show_mouse, "timeout", timeout,
		NULL);

	if (win->error != NULL) {
		g_propagate_error(error, win->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(win);
		gtk_window_destroy(GTK_WINDOW(win));
		return NULL;
	}

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));

	// GTK always return GtkWidget, so do I.
	return GTK_WIDGET(win);
}

void smtk_keys_win_hide(SmtkKeysWin *win)
{
	g_return_if_fail(win != NULL);

	smtk_keys_emitter_pause(win->emitter);
	gtk_widget_hide(GTK_WIDGET(win));
}

void smtk_keys_win_show(SmtkKeysWin *win)
{
	g_return_if_fail(win != NULL);

	gtk_widget_show(GTK_WIDGET(win));
	smtk_keys_emitter_resume(win->emitter);
}

