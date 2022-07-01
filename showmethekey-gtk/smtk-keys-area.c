#include "smtk-keys-area.h"

struct _SmtkKeysArea {
	GtkDrawingArea parent_instance;

	GSList *keys;
	// Use single list because we only do prepend and iter, it's easier to
	// break;
	GMutex keys_mutex;

	PangoLayout *layout;
	PangoFontDescription *font;
	cairo_t *cr;
	int key_margin;
	int key_padding;
	int key_height;
	int last_key_x;
	int width;
	int height;

	GThread *timer_thread;
	bool timer_running;
	GTimer *timer;
	int timeout;
};
G_DEFINE_TYPE(SmtkKeysArea, smtk_keys_area, GTK_TYPE_DRAWING_AREA)

enum { PROP_0, PROP_TIMEOUT, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

static void smtk_keys_area_set_property(GObject *object,
					unsigned int property_id,
					const GValue *value, GParamSpec *pspec)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	switch (property_id) {
	case PROP_TIMEOUT:
		area->timeout = g_value_get_int(value);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_area_get_property(GObject *object,
					unsigned int property_id, GValue *value,
					GParamSpec *pspec)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	switch (property_id) {
	case PROP_TIMEOUT:
		g_value_set_int(value, area->timeout);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_area_draw_key(SmtkKeysArea *area, const char key[])
{
	g_debug("Drawing key: %s\n", key);

	cairo_set_source_rgba(area->cr, 1.0, 1.0, 1.0, 1.0);

	// See <https://docs.gtk.org/Pango/method.Layout.set_text.html#parameters>.
	pango_layout_set_text(area->layout, key, -1);
	PangoRectangle ink;
	pango_layout_get_pixel_extents(area->layout, &ink, NULL);

	int border_height = area->key_height;
	int border_width = ink.width + area->key_padding * 2;
	int border_y = (area->height - area->key_height) / 2;
	int border_x = area->last_key_x - (area->key_margin + border_width);
	cairo_rectangle(area->cr, border_x, border_y, border_width,
			border_height);
	cairo_set_line_width(area->cr, area->key_padding / 5.0);
	cairo_stroke(area->cr);

	// See <https://mail.gnome.org/archives/gtk-devel-list/2001-August/msg00325.html>.
	// ink rectangle:  x      = -4
	//                 y      = -9
	//                 width  = 31
	//                 height = 19
	// (Width and height seems not correct but not a problem.)
	//
	// ---------------------------------
	// |            __aaaaaas,,        |
	// |         awQ ?^   ~?WQQQQQQQQW`|
	// |       aQWWF        )WQQQT???^ |
	// |     .mQQQF          QQQQ[     |
	// |     jQQQ@          <QQQQ[     |
	// |     QQQQf          dQQQ@      |
	// |     4QQQL         jQQQP`      |
	// |      4QQQc      _yQQV^        |
	// |    |  -?9WmwaaadT?"`          |
	// -----+---------------------------
	// |    | _mQQ;                    |
	// |      QQQQQgaa,,,              |
	// |      )QQQQQQQQQQQmya,.        |
	// |  .<wmT^ --"?TT$QQQQQQQa       |
	// | jQQD'           -"?$QQQr      |
	// |dQQW`                4QQ(      |
	// |$QQQ,                jQF       |
	// |-9QQQa.           _sdT'        |
	// |   "?YVGwaaaaa%%%?!`           |
	// ---------------------------------
	//
	// Pango draws text from reference point.
	// 1: centered box topleft is (border_width - ink.width) / 2 + border_x,
	//    border_y + (border_height - ink.height) / 2.
	// 2: reference point is topleft.x - ink.x, topleft.y - ink.y.
	// Note that ink.height is not always font_size, so we cannot use
	// key_padding for vertically center.
	// TODO: Center place is still not looks good, maybe I need some
	// baseline placement but not too high.
	cairo_move_to(area->cr, border_x + area->key_padding - ink.x,
		      border_y + (border_height - ink.height) / 2.0 - ink.y);
	pango_cairo_show_layout(area->cr, area->layout);

	area->last_key_x -= (border_width + area->key_margin);
}

static void smtk_keys_area_draw(GtkDrawingArea *drawing_area, cairo_t *cr,
				int width, int height, gpointer user_data)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(drawing_area);
	area->cr = cr;
	if (area->layout)
		pango_cairo_update_layout(cr, area->layout);
	else
		area->layout = pango_cairo_create_layout(cr);
	pango_layout_set_ellipsize(area->layout, PANGO_ELLIPSIZE_NONE);
	area->width = width;
	area->height = height;
	area->key_height = height * 0.8;
	int font_size = area->key_height * 0.8;
	area->key_margin = area->key_height * 0.4;
	area->key_padding = (area->key_height - font_size) / 2;
	area->last_key_x = width;
	pango_font_description_set_absolute_size(area->font,
						 font_size * PANGO_SCALE);
	pango_layout_set_font_description(area->layout, area->font);

	g_mutex_lock(&area->keys_mutex);
	for (GSList *iter = area->keys; iter; iter = iter->next) {
		if (area->last_key_x < 0) {
			// We cannot handle next one, but it's OK to break
			// current one.
			GSList *unused = iter->next;
			iter->next = NULL;
			if (unused != NULL)
				g_slist_free_full(g_steal_pointer(&unused),
						  g_free);
			break;
		}
		smtk_keys_area_draw_key(area, iter->data);
	}
	// g_print("%d\n", g_slist_length(area->keys));
	g_mutex_unlock(&area->keys_mutex);
}

static void idle_destroy_function(gpointer user_data)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(user_data);
	g_object_unref(area);
}

// true and false are C99 _Bool, but GLib expects gboolean, which is C99 int.
static int idle_function(gpointer user_data)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(user_data);

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(area));

	return 0;
}

static gpointer timer_function(gpointer user_data)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(user_data);

	while (area->timer_running) {
		g_mutex_lock(&area->keys_mutex);
		int elapsed = g_timer_elapsed(area->timer, NULL) * 1000.0;
		g_mutex_unlock(&area->keys_mutex);

		if (area->timeout > 0 && elapsed > area->timeout) {
			g_mutex_lock(&area->keys_mutex);
			if (area->keys != NULL)
				g_slist_free_full(g_steal_pointer(&area->keys),
						  g_free);
			g_timer_start(area->timer);
			g_mutex_unlock(&area->keys_mutex);

			// Here is not UI thread so we need to kick an async
			// callback into GLib's main loop.
			g_timeout_add_full(G_PRIORITY_DEFAULT, 0, idle_function,
					   g_object_ref(area),
					   idle_destroy_function);
		}

		g_usleep(1000L);
	}

	return NULL;
}

static void smtk_keys_area_init(SmtkKeysArea *area)
{
	area->layout = NULL;
	area->font = pango_font_description_new();
	pango_font_description_set_family(area->font, "monospace");
	area->cr = NULL;
	area->key_height = 0;
	area->width = 0;
	area->height = 0;
	area->keys = NULL;
	g_mutex_init(&area->keys_mutex);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area),
				       smtk_keys_area_draw, NULL, NULL);

	area->timer_running = true;
	area->timer = g_timer_new();

	// Just ignore timer thread error because it should work without timer.
	area->timer_thread =
		g_thread_try_new("timer", timer_function, area, NULL);
}

static void smtk_keys_area_dispose(GObject *object)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	g_clear_object(&area->layout);
	g_clear_pointer(&area->font, pango_font_description_free);

	if (area->timer_thread != NULL) {
		area->timer_running = false;
		g_thread_join(area->timer_thread);
		area->timer_thread = NULL;
	}

	g_clear_pointer(&area->timer, g_timer_destroy);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->dispose(object);
}

static void smtk_keys_area_finalize(GObject *object)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	if (area->keys != NULL)
		g_slist_free_full(g_steal_pointer(&area->keys), g_free);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->finalize(object);
}

static void smtk_keys_area_class_init(SmtkKeysAreaClass *area_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(area_class);

	object_class->set_property = smtk_keys_area_set_property;
	object_class->get_property = smtk_keys_area_get_property;

	object_class->dispose = smtk_keys_area_dispose;
	object_class->finalize = smtk_keys_area_finalize;

	obj_properties[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

GtkWidget *smtk_keys_area_new(int timeout)
{
	SmtkKeysArea *area =
		g_object_new(SMTK_TYPE_KEYS_AREA, "timeout", timeout, NULL);
	return GTK_WIDGET(area);
}

void smtk_keys_area_add_key(SmtkKeysArea *area, char key[])
{
	g_return_if_fail(area != NULL);

	g_debug("Adding key: %s\n", key);
	g_mutex_lock(&area->keys_mutex);
	area->keys = g_slist_prepend(area->keys, key);
	g_timer_start(area->timer);
	g_mutex_unlock(&area->keys_mutex);

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(area));
}
