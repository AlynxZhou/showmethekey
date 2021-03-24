#include <gtk/gtk.h>
#include <gio/gio.h>

#include "smtk-keys-win.h"

struct _SmtkKeysWin {
	GtkWindow parent_instance;
	GtkWidget *header_bar;
	int header_bar_height;
	GtkWidget *keys_label;
	GSubprocess *cli;
	GDataInputStream *cli_out;
	GThread *poller;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

struct update_data {
	SmtkKeysWin *win;
	char *line;
};

static void smtk_keys_win_on_update_label(gpointer user_data)
{
	// TODO: Now we have events in JSON, need to make a list of keys.
	struct update_data *update_data = user_data;
	gtk_label_set_text(GTK_LABEL(update_data->win->keys_label),
			   update_data->line);
	g_free(update_data->line);
	g_free(update_data);
}

static gpointer poller_function(gpointer user_data)
{
	SmtkKeysWin *win = SMTK_KEYS_WIN(user_data);
	while (TRUE) {
		GError *read_line_error = NULL;
		char *line = g_data_input_stream_read_line(
			win->cli_out, NULL, NULL, &read_line_error);
		// See <https://developer.gnome.org/gio/2.56/GDataInputStream.html#g-data-input-stream-read-line>.
		if (line == NULL && read_line_error != NULL) {
			g_error("Read line error: %s.\n",
				read_line_error->message);
			g_error_free(read_line_error);
			continue;
		}
		// g_print("Backend JSON output: %s.\n", line);
		// UI can only be modified in UI thread, and we are not in UI
		// thread here. So we need to use `g_idle_add()` to kick an
		// async callback into glib's main loop (the same as GTK UI
		// thread).
		// Signals are not async! So we cannot use signal callback here,
		// because they will run in poller thread, too.
		// TODO: Better user_data handling?
		struct update_data *update_data =
			g_malloc(sizeof(*update_data));
		update_data->win = win;
		update_data->line = g_strdup(line);
		g_idle_add(G_SOURCE_FUNC(smtk_keys_win_on_update_label),
			   update_data);
		g_free(line);
	}
	return NULL;
}

static void smtk_keys_win_header_bar_on_size_allocate(SmtkKeysWin *win,
						      GdkRectangle *allocation,
						      GtkWidget *header_bar)
{
	// Widget's allocation is only usable after realize.
	// However, the first allocation we get in realize might be not correct.
	// So we have to connect to header_bar's size-allocation signal and
	// update input shape with it.
	win->header_bar_height = allocation->height;
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
	pango_font_description_set_absolute_size(
		font, (allocation->height - win->header_bar_height) * 0.5 *
			      PANGO_SCALE);

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
	// gtk_widget_init_template(GTK_WIDGET(win));

	// Allow user to choose position by drag this.
	win->header_bar = gtk_header_bar_new();
	win->header_bar_height = 0;
	// Disable subtitle to get a compact header bar.
	gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(win->header_bar), FALSE);
	gtk_window_set_titlebar(GTK_WINDOW(win), win->header_bar);
	// We need to mark widget as visible manually in GTK3.
	gtk_widget_show(win->header_bar);

	// CSS is used to make a transparent titlebar,
	// because it's above window.
	win->keys_label = gtk_label_new("");
	// If there are too much keys, we hide older keys.
	gtk_label_set_ellipsize(GTK_LABEL(win->keys_label),
				PANGO_ELLIPSIZE_START);
	// Center is better for user.
	gtk_label_set_justify(GTK_LABEL(win->keys_label), GTK_JUSTIFY_CENTER);
	// This prevent label from resizing window. But it is confusing,
	// it does not limit the label to only 1 char, which is I want.
	// See <https://developer.gnome.org/gtk3/stable/GtkLabel.html#label-text-layout>.
	gtk_label_set_max_width_chars(GTK_LABEL(win->keys_label), 1);
	// We should update label size on window's `size-allocation` signal.
	gtk_container_add(GTK_CONTAINER(win), win->keys_label);
	gtk_widget_show(win->keys_label);

	g_signal_connect(win, "draw", G_CALLBACK(smtk_keys_win_on_draw), NULL);
	g_signal_connect_object(
		win->header_bar, "size-allocate",
		G_CALLBACK(smtk_keys_win_header_bar_on_size_allocate), win,
		G_CONNECT_SWAPPED);
	g_signal_connect(win, "size-allocate",
			 G_CALLBACK(smtk_keys_win_on_size_allocate), NULL);

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

	GError *subprocess_error = NULL;
	win->cli = g_subprocess_new(
		G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
		&subprocess_error, "pkexec",
		"/home/alynx/Projects/showmethekey/build/showmethekey-cli",
		NULL);
	if (win->cli == NULL) {
		g_error("Spawn subprocess error: %s.\n",
			subprocess_error->message);
		// TODO: Turn off switch if error.
		g_error_free(subprocess_error);
	}
	// Actually I don't wait the subprocess to return, they work like
	// clients and daemons, why clients want to wait for daemons' exiting?
	// This is just spawn subprocess.
	g_subprocess_wait_async(win->cli, NULL, NULL, NULL);
	win->cli_out =
		g_data_input_stream_new(g_subprocess_get_stdout_pipe(win->cli));

	GError *thread_error = NULL;
	win->poller =
		g_thread_try_new("poller", poller_function, win, &thread_error);
	if (win->poller == NULL) {
		g_error("Run thread error: %s.\n", thread_error->message);
		// TODO: Turn off switch if error.
		g_error_free(thread_error);
	}
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
		"width-request", 1400, "height-request", 200, "can-focus",
		FALSE, "focus-on-click", FALSE, "focus-on-map", FALSE,
		"accept-focus", FALSE, "app-paintable", TRUE, "vexpand", FALSE,
		"vexpand-set", TRUE, "hexpand", FALSE, "hexpand-set", TRUE,
		// We cannot focus on this window, and it has no border,
		// so user resize is meaningless for it.
		"resizable", FALSE, "skip-pager-hint", TRUE,
		"skip-taskbar-hint", TRUE, NULL);
}

GtkWidget *smtk_keys_win_get_keys_label(SmtkKeysWin *win)
{
	return win->keys_label;
}
