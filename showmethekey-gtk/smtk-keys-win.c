#include <gtk/gtk.h>
// #include <gdk/x11/gdkx.h>

#include "smtk-keys-win.h"

// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
// #include <X11/Xatom.h>
//
// #include <X11/extensions/shape.h>


struct _SmtkKeysWin {
	GtkWindow parent_instance;
	GtkWidget *keys_label;
};
G_DEFINE_TYPE(SmtkKeysWin, smtk_keys_win, GTK_TYPE_WINDOW)

// TODO: Make a frameless window.

static void smtk_keys_win_on_realize(SmtkKeysWin *win)
{
	// cairo_rectangle_int_t empty_rect = {0, 0, 1, 1};
	// cairo_region_t *empty_region = cairo_region_create_rectangle(&empty_rect);
	GtkNative *native = gtk_widget_get_native(GTK_WIDGET(win));
	if (native != NULL) {
		// g_print("empty? %d\n", cairo_region_is_empty(empty_region));
		// g_print("focusable? %d\n", gtk_widget_get_focusable(GTK_WIDGET(win)));
		// g_print("can_focus? %d\n", gtk_widget_get_can_focus(GTK_WIDGET(win)));
		// g_print("focus_on_click? %d\n", gtk_widget_get_focus_on_click(GTK_WIDGET(win)));
		// TODO: Not work.
		GdkSurface *surface = gtk_native_get_surface(native);
		gdk_surface_set_input_region(surface, NULL);
		// XShapeCombineMask (GDK_DISPLAY_XDISPLAY (gdk_surface_get_display(surface)),
                //              GDK_SURFACE_XID (surface),
                //              ShapeInput,
                //              0, 0,
                //              None,
                //              ShapeSet);
	}
	// cairo_region_destroy(empty_region);
}

static void smtk_keys_win_init(SmtkKeysWin *win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void smtk_keys_win_class_init(SmtkKeysWinClass *win_class)
{
	gtk_widget_class_set_template_from_resource(
		GTK_WIDGET_CLASS(win_class),
		"/one/alynx/showmethekey/smtk-keys-win.ui");
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(win_class), SmtkKeysWin, keys_label);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(win_class), smtk_keys_win_on_realize);
}

GtkWidget *smtk_keys_win_new(void)
{
	return g_object_new(SMTK_TYPE_KEYS_WIN, NULL);
}

GtkWidget *smtk_keys_win_get_keys_label(SmtkKeysWin *win)
{
	return win->keys_label;
}
