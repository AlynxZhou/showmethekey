#include <gtk/gtk.h>
#include <gio/gio.h>

#include "smtk-keys-win.h"

struct _SmtkKeysWin {
	GtkWindow parent_instance;
	GtkWidget *header_bar;
	GtkWidget *keys_label;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

static void smtk_keys_win_on_header_bar_size_allocate(SmtkKeysWin *win,
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
	// gtk_widget_init_template(GTK_WIDGET(win));

	// Allow user to choose position by drag this.
	win->header_bar = gtk_header_bar_new();
	gtk_widget_set_visible(win->header_bar, TRUE);
	// Disable subtitle to get a compact header bar.
	gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(win->header_bar), FALSE);
	gtk_window_set_titlebar(GTK_WINDOW(win), win->header_bar);

	// CSS is used to make a transparent titlebar,
	// because it's above window.
	win->keys_label = gtk_label_new(NULL);
	gtk_widget_set_visible(win->keys_label, TRUE);
	gtk_container_add(GTK_CONTAINER(win), win->keys_label);

	g_signal_connect(win, "draw", G_CALLBACK(smtk_keys_win_on_draw), NULL);
	g_signal_connect_object(
		win->header_bar, "size-allocate",
		G_CALLBACK(smtk_keys_win_on_header_bar_size_allocate), win,
		G_CONNECT_SWAPPED);

	// TODO: Those only works for GNOME X11 session,
	// for GNOME Wayland session, show some message to let user click
	// app menu by themselves.
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_stick(GTK_WINDOW(win));

	// We can not make a half transparent window with CSS,
	// and need to set visual and do custom draw.
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
	GtkStyleContext *headerbar_style_context =
		gtk_widget_get_style_context(win->header_bar);
	gtk_style_context_add_provider(
		headerbar_style_context,
		GTK_STYLE_PROVIDER(header_bar_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void smtk_keys_win_class_init(SmtkKeysWinClass *win_class)
{
	// It seems a widget from `.ui` file is unable to set to transparent.
	// So we have to make UI from code.
	// G_OBJECT_CLASS(win_class)->dispose = smtk_keys_win_dispose;

	// gtk_widget_class_set_template_from_resource(
	// GTK_WIDGET_CLASS(win_class),
	// "/one/alynx/showmethekey/smtk-keys-win.ui");
	// gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class), SmtkKeysWin, keys_label);
	// gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(win_class), smtk_keys_win_on_configure);
	// gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(win_class), smtk_keys_win_on_draw);

	// TODO: Release things in dispose?
}

GtkWidget *smtk_keys_win_new(void)
{
	return g_object_new(
		SMTK_TYPE_KEYS_WIN, "visible", TRUE, "title", "Show Me The Key",
		"width-request", 800, "height-request", 200, "can-focus", FALSE,
		"focus-on-click", FALSE, "focus-on-map", FALSE, "accept-focus",
		FALSE, "app-paintable", TRUE,
		// We cannot focus on this window, and it has no border,
		// so user resize is meaningless for it.
		"resizable", FALSE, "skip-pager-hint", TRUE,
		"skip-taskbar-hint", TRUE, NULL);
}

GtkWidget *smtk_keys_win_get_keys_label(SmtkKeysWin *win)
{
	return win->keys_label;
}
