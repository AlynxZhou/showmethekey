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
	SmtkKeysEmitter *emitter;
	SmtkKeyMode mode;
	gboolean show_mouse;
	gint timeout;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

enum { PROP_0, PROP_MODE, PROP_SHOW_MOUSE, PROP_TIMEOUT, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static void smtk_keys_win_set_property(GObject *object, guint property_id,
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

static void smtk_keys_win_get_property(GObject *object, guint property_id,
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
	// It seems calling g_object_unref() on gtk_widget is not enough?
	// We need to call gtk_widget_destroy().
	gtk_widget_destroy(GTK_WIDGET(win));
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

static void smtk_keys_win_handle_on_size_allocate(SmtkKeysWin *win,
						  GdkRectangle *allocation,
						  GtkWidget *header_bar)
{
	// Widget's allocation is only usable after realize.
	// However, the first allocation we get in realize might be not correct.
	// So we have to connect to header_bar's size-allocation signal and
	// update input shape with it.
	cairo_region_t *clickable_region =
		cairo_region_create_rectangle(allocation);
	// GtkNative *native = gtk_widget_get_native(GTK_WIDGET(win));
	// if (native != NULL) {
	// GdkSurface *surface = gtk_native_get_surface(native);
	// gdk_surface_set_input_region(surface, NULL);
	// }
	gtk_widget_input_shape_combine_region(GTK_WIDGET(win),
					      clickable_region);
	cairo_region_destroy(clickable_region);
}

static void smtk_keys_win_on_size_allocate(SmtkKeysWin *win,
					   GdkRectangle *allocation,
					   gpointer user_data)
{
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(win->keys_label));

	PangoFontDescription *font = pango_font_description_new();
	pango_font_description_set_family(font, "monospace");
	// I am not sure why the avaliable height * PANGO_SCALE is too big,
	// just make it smaller, also too big will have less chars.
	pango_font_description_set_size(font,
					allocation->height * 0.3 * PANGO_SCALE);

	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
}

static gboolean smtk_keys_win_on_draw(SmtkKeysWin *win, cairo_t *cr,
				      gpointer user_data)
{
	cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	return FALSE;
}

static void smtk_keys_win_init(SmtkKeysWin *win)
{
	// It seems a widget from `.ui` file is unable to set to transparent.
	// So we have to make UI from code.
	win->error = NULL;

	// Allow user to choose position by drag this.
	win->header_bar = gtk_header_bar_new();
	// Disable subtitle to get a compact header bar.
	gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(win->header_bar), FALSE);
	win->handle = gtk_label_new(_("Clickable Area"));
	gtk_header_bar_set_custom_title(GTK_HEADER_BAR(win->header_bar),
					win->handle);
	gtk_widget_show(win->handle);
	gtk_window_set_titlebar(GTK_WINDOW(win), win->header_bar);
	// We need to mark widget as visible manually in GTK3.
	gtk_widget_show(win->header_bar);

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
	gtk_label_set_use_markup(GTK_LABEL(win->keys_label), TRUE);
	// We should update label size on window's `size-allocation` signal.
	gtk_container_add(GTK_CONTAINER(win), win->keys_label);
	gtk_widget_show(win->keys_label);

	g_signal_connect(win, "draw", G_CALLBACK(smtk_keys_win_on_draw), NULL);
	g_signal_connect_object(
		win->handle, "size-allocate",
		G_CALLBACK(smtk_keys_win_handle_on_size_allocate), win,
		G_CONNECT_SWAPPED);
	g_signal_connect(win, "size-allocate",
			 G_CALLBACK(smtk_keys_win_on_size_allocate), NULL);

	// GTK4 dropped those API, so we need to implement
	// those when upgrading GTK4 by ourselves via WM hints.
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_stick(GTK_WINDOW(win));

	// We can not make a half transparent window with CSS,
	// and need to set visual and do custom draw.
	// GTK4 dropped this and we may update it.
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(win));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	if (visual != NULL)
		gtk_widget_set_visual(GTK_WIDGET(win), visual);

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

	// It seems that GTK3's gtk_widget_destroy() will automatically drop
	// reference to children, while GTK4's gtk_window_destroy() not,
	// so we will add children unparent code here in future.

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

	object_class->set_property = smtk_keys_win_set_property;
	object_class->get_property = smtk_keys_win_get_property;

	object_class->constructed = smtk_keys_win_constructed;

	object_class->dispose = smtk_keys_win_dispose;

	obj_properties[PROP_MODE] =
		g_param_spec_enum("mode", "Mode", "Key Mode",
				  SMTK_TYPE_KEY_MODE, SMTK_KEY_MODE_COMPOSED,
				  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	obj_properties[PROP_SHOW_MOUSE] = g_param_spec_boolean(
		"show-mouse", "Show Mouse", "Show Mouse Button", TRUE,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	obj_properties[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

GtkWidget *smtk_keys_win_new(gboolean show_mouse, SmtkKeyMode mode,
			     guint64 width, guint64 height, gint timeout,
			     GError **error)
{
	SmtkKeysWin *win = g_object_new(
		// Don't translate floating window's title, maybe users have
		// window rules for it.
		SMTK_TYPE_KEYS_WIN, "visible", TRUE, "title",
		"Floating Window - Show Me The Key", "icon-name",
		"one.alynx.showmethekey", "width-request", width,
		"height-request", height, "can-focus", FALSE, "focus-on-click",
		FALSE,
		// This window is able to be focused, so this prevent that when
		// you start it and press Enter, and focus is on the app window,
		// and it closes.
		"focus-on-map", TRUE, "accept-focus", TRUE,
		// Must be paintable for a transparent window.
		"app-paintable", TRUE, "vexpand", FALSE, "vexpand-set", TRUE,
		"hexpand", FALSE, "hexpand-set", TRUE,
		// We cannot focus on this window, and it has no border,
		// so user resize is meaningless for it.
		"resizable", FALSE,
		// Wayland does not support this, it's ok.
		// "skip-pager-hint", TRUE, "skip-taskbar-hint", TRUE,
		"mode", mode, "show-mouse", show_mouse, "timeout", timeout,
		NULL);

	if (win->error != NULL) {
		g_propagate_error(error, win->error);
		// GtkWidget is GInitiallyUnowned,
		// so we need to sink the floating first.
		g_object_ref_sink(win);
		gtk_widget_destroy(GTK_WIDGET(win));
		return NULL;
	}

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
