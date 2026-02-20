#include <gtk/gtk.h>

#include "smtk-enum-types.h"
#include "smtk-keys-area.h"
#include "smtk-keys-mapper.h"

struct key_data {
	char *string;
	int counter;
};

struct _SmtkKeysArea {
	GtkDrawingArea parent_instance;
	GSettings *settings;

	SmtkKeyMode mode;
	SmtkKeyAlignment alignment;
	GSList *keys;
	// Use single list because we only prepend and iter.
	GMutex keys_mutex;

	PangoLayout *string_layout;
	PangoFontDescription *string_font;
	PangoLayout *counter_layout;
	PangoFontDescription *counter_font;
	int width;
	int height;
	int string_height;
	int counter_height;
	double margin_ratio;
	int margin;
	int padding;
	int last_key_x;
	bool draw_border;

	GThread *timer_thread;
	bool timer_running;
	GTimer *timer;
	int timeout;
};
G_DEFINE_TYPE(SmtkKeysArea, smtk_keys_area, GTK_TYPE_DRAWING_AREA)

enum {
	PROP_0,
	PROP_MODE,
	PROP_ALIGNMENT,
	PROP_DRAW_BORDER,
	PROP_MARGIN_RATIO,
	PROP_TIMEOUT,
	N_PROPS
};

static GParamSpec *obj_props[N_PROPS] = { NULL };

void smtk_keys_area_set_timeout(SmtkKeysArea *area, int timeout)
{
	g_return_if_fail(area != NULL);

	area->timeout = timeout;
}

static void smtk_keys_area_set_property(GObject *object,
					unsigned int property_id,
					const GValue *value, GParamSpec *pspec)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	switch (property_id) {
	case PROP_MODE:
		area->mode = g_value_get_enum(value);
		break;
	case PROP_ALIGNMENT:
		area->alignment = g_value_get_enum(value);
		// Trigger re-draw because alignment changed.
		gtk_widget_queue_draw(GTK_WIDGET(area));
		break;
	case PROP_DRAW_BORDER:
		area->draw_border = g_value_get_boolean(value);
		// Trigger re-draw because border changed.
		gtk_widget_queue_draw(GTK_WIDGET(area));
		break;
	case PROP_MARGIN_RATIO:
		area->margin_ratio = g_value_get_double(value);
		// Trigger re-draw because margin changed.
		gtk_widget_queue_draw(GTK_WIDGET(area));
		break;
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
	case PROP_MODE:
		g_value_set_enum(value, area->mode);
		break;
	case PROP_ALIGNMENT:
		g_value_set_enum(value, area->alignment);
		break;
	case PROP_DRAW_BORDER:
		g_value_set_boolean(value, area->draw_border);
		break;
	case PROP_MARGIN_RATIO:
		g_value_set_double(value, area->margin_ratio);
		break;
	case PROP_TIMEOUT:
		g_value_set_int(value, area->timeout);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static int smtk_keys_area_calculate_width(SmtkKeysArea *area)
{
	int result = 0;

	g_mutex_lock(&area->keys_mutex);
	for (GSList *iter = area->keys; iter; iter = iter->next) {
		if (result > area->width)
			break;

		const struct key_data *key_data = iter->data;
		char *string = g_strdup(key_data->string);
		char *counter = g_strdup_printf("×%d", key_data->counter);

		PangoRectangle s_ink;
		pango_layout_set_text(area->string_layout, string, -1);
		pango_layout_get_pixel_extents(area->string_layout, &s_ink,
					       NULL);
		result += (s_ink.width + area->padding * 2);

		if (key_data->counter > 1) {
			PangoRectangle c_ink;
			pango_layout_set_text(area->counter_layout, counter,
					      -1);
			pango_layout_get_pixel_extents(area->counter_layout,
						       &c_ink, NULL);
			result += (c_ink.width + area->padding * 2);
		}

		if (iter->next != NULL)
			result += area->margin;

		g_free(string);
		g_free(counter);
	}
	g_mutex_unlock(&area->keys_mutex);

	return result;
}

static void smtk_keys_area_draw_key(SmtkKeysArea *area, cairo_t *cr,
				    const struct key_data *key_data)
{
	char *string = g_strdup(key_data->string);
	char *counter = g_strdup_printf("×%d", key_data->counter);

	if (key_data->counter > 1)
		g_debug("Drawing key: %s%s.", string, counter);
	else
		g_debug("Drawing key: %s.", string);

	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

	// See <https://docs.gtk.org/Pango/method.Layout.set_text.html#parameters>.
	PangoRectangle s_ink;
	PangoRectangle s_logical;
	pango_layout_set_text(area->string_layout, string, -1);
	pango_layout_get_pixel_extents(area->string_layout, &s_ink, &s_logical);
	g_debug("Ink: x is %d, y is %d, width is %d, height is %d.", s_ink.x,
		s_ink.y, s_ink.width, s_ink.height);
	g_debug("Logical: x is %d, y is %d, width is %d, height is %d.",
		s_logical.x, s_logical.y, s_logical.width, s_logical.height);

	int border_height = area->string_height;
	int border_width = s_ink.width + area->padding * 2;
	int border_y = (area->height - area->string_height) / 2;
	int border_x = area->last_key_x - border_width;

	PangoRectangle c_ink;
	if (key_data->counter > 1) {
		pango_layout_set_text(area->counter_layout, counter, -1);
		pango_layout_get_pixel_extents(area->counter_layout, &c_ink,
					       NULL);

		// Don't forget to leave space for counter.
		border_x -= (c_ink.width + area->padding * 2);
	}

	g_debug("Border: x is %d, y is %d, width is %d, height is %d.",
		border_x, border_y, border_width, border_height);

	if (area->draw_border) {
		// See <https://sh.alynx.one/posts/Ink-and-Logical-Rectangles-of-Pango/>.
		//
		// We align keys by borders, so move texts into borders. To
		// archive this, we are treating glyphs as images instead of
		// texts, because texts are aligned via baseline, which means
		// some glyphs can be drawn outside the given border because the
		// given border is treated as baseline. The logical rectangle is
		// used to align glyphs as texts on baseline, which is not
		// useful to us. The ink rectangle is the bounding box of
		// glphys, which is the one we need.
		//
		// See <https://docs.gtk.org/PangoCairo/func.show_layout.html>.
		//
		// `pango_cairo_show_layout()` draw from the top-left corner of
		// the logical rectangle, not the ink rectangle. The x and y
		// coordinates of the ink rectangle are the offset to the
		// top-left corner of the logical rectangle, so we use them to
		// offset the draw point.
		//
		// For example:
		// String font size: 97.
		// Drawing key: g.
		// Ink: x is -1, y is 50, width is 60, height is 73.
		// Logical: x is 0, y is 0, width is 55, height is 129.
		// Border: x is 2428, y is 15, width is 84, height is 122.
		//
		// Finally we will have a center-bottom alignment.
		const int x = border_x + area->padding - s_ink.x;
		const int y = border_y + border_height - area->padding -
			      s_ink.height - s_ink.y;
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, area->string_layout);
		// Draw border.
		cairo_set_line_width(cr, area->padding / 5.0);
		cairo_rectangle(cr, border_x, border_y, border_width,
				border_height);
		cairo_stroke(cr);
	} else {
		// When no border, just let Pango align texts so they will
		// sit on the baseline. But some icons are too large so if they
		// are aligned by baseline, they will draw outside area, so we
		// use ink rectangle for them to align glyphs by bottom.
		//
		// Finally we get a center-baseline or center-bottom alignment
		// so keys won't move horizontally when toggling borders.
		const int x = border_x + area->padding - s_ink.x;
		int y = border_y - s_logical.y;
		if (y + s_logical.height > area->height) {
			g_debug("%s will draw outside area if aligned by "
				"baseline, will be aligned by bottom.",
				string);
			y = border_y + border_height - area->padding -
			    s_ink.height - s_ink.y;
		}
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, area->string_layout);
	}

	if (key_data->counter > 1) {
		// We always use the ink rectangles to align counters because
		// they are all similiar and this is simple.
		const int x = border_x + border_width + area->padding - c_ink.x;
		const int y = border_y + border_height - c_ink.height - c_ink.y;
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, area->counter_layout);
	}

	area->last_key_x = border_x - area->margin;

	g_free(string);
	g_free(counter);
}

static void smtk_keys_area_draw(GtkDrawingArea *drawing_area, cairo_t *cr,
				int width, int height, void *data)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(drawing_area);

	area->width = width;
	area->height = height;

	if (area->string_layout) {
		pango_cairo_update_layout(cr, area->string_layout);
	} else {
		area->string_layout = pango_cairo_create_layout(cr);
		pango_layout_set_ellipsize(area->string_layout,
					   PANGO_ELLIPSIZE_NONE);
	}

	area->string_height = area->height * 0.8;
	const int string_font_size = area->string_height * 0.9;
	g_debug("String font size: %d.", string_font_size);
	pango_font_description_set_absolute_size(
		area->string_font, string_font_size * PANGO_SCALE);
	pango_layout_set_font_description(area->string_layout,
					  area->string_font);

	if (area->counter_layout) {
		pango_cairo_update_layout(cr, area->counter_layout);
	} else {
		area->counter_layout = pango_cairo_create_layout(cr);
		pango_layout_set_ellipsize(area->counter_layout,
					   PANGO_ELLIPSIZE_NONE);
	}

	area->counter_height = area->string_height * 0.5;
	const int counter_font_size = area->counter_height * 0.8;
	g_debug("Counter font size: %d.", string_font_size);
	pango_font_description_set_absolute_size(
		area->counter_font, counter_font_size * PANGO_SCALE);
	pango_layout_set_font_description(area->counter_layout,
					  area->counter_font);

	area->margin = area->string_height * area->margin_ratio;
	area->padding = (area->string_height - string_font_size) / 2;

	// To align to center, we need to first calculate total width.
	if (area->alignment == SMTK_KEY_ALIGNMENT_END) {
		area->last_key_x = area->width;
	} else {
		const int keys_width = smtk_keys_area_calculate_width(area);
		if (keys_width > area->width)
			area->last_key_x = area->width;
		else
			area->last_key_x =
				(area->width - keys_width) / 2 + keys_width;
	}

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
		smtk_keys_area_draw_key(area, cr, iter->data);
	}
	g_debug("Keys list length: %d", g_slist_length(area->keys));
	g_mutex_unlock(&area->keys_mutex);
}

static void idle_destroy_function(void *data)
{
	SmtkKeysArea *area = data;
	g_object_unref(area);
}

// true and false are C99 _Bool, but GLib expects gboolean, which is C99 int.
static int idle_function(void *data)
{
	SmtkKeysArea *area = data;

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(area));

	return 0;
}

static void *timer_function(void *data)
{
	SmtkKeysArea *area = data;

	while (area->timer_running) {
		g_mutex_lock(&area->keys_mutex);
		int elapsed = g_timer_elapsed(area->timer, NULL) * 1000.0;
		g_mutex_unlock(&area->keys_mutex);

		if (area->timeout > 0 && elapsed > area->timeout) {
			g_debug("Timer triggered, clear keys.");
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
	area->settings = NULL;
	area->width = 0;
	area->height = 0;

	area->string_layout = NULL;
	area->string_font = pango_font_description_new();
	pango_font_description_set_family(area->string_font, "monospace");
	area->string_height = 0;

	area->counter_layout = NULL;
	area->counter_font = pango_font_description_new();
	pango_font_description_set_family(area->counter_font, "monospace");
	area->counter_height = 0;

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

static void smtk_keys_area_constructed(GObject *object)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	area->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(area->settings, "mode", area, "mode",
			G_SETTINGS_BIND_GET);
	g_settings_bind(area->settings, "alignment", area, "alignment",
			G_SETTINGS_BIND_GET);
	g_settings_bind(area->settings, "draw-border", area, "draw-border",
			G_SETTINGS_BIND_GET);
	g_settings_bind(area->settings, "margin-ratio", area, "margin-ratio",
			G_SETTINGS_BIND_GET);
	g_settings_bind(area->settings, "timeout", area, "timeout",
			G_SETTINGS_BIND_GET);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->constructed(object);
}

static void smtk_keys_area_dispose(GObject *object)
{
	SmtkKeysArea *area = SMTK_KEYS_AREA(object);

	g_clear_object(&area->settings);

	g_clear_object(&area->string_layout);
	g_clear_pointer(&area->string_font, pango_font_description_free);

	g_clear_object(&area->counter_layout);
	g_clear_pointer(&area->counter_font, pango_font_description_free);

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

	g_clear_slist(&area->keys, g_free);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->finalize(object);
}

static void smtk_keys_area_class_init(SmtkKeysAreaClass *area_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(area_class);

	object_class->constructed = smtk_keys_area_constructed;

	object_class->set_property = smtk_keys_area_set_property;
	object_class->get_property = smtk_keys_area_get_property;

	object_class->dispose = smtk_keys_area_dispose;
	object_class->finalize = smtk_keys_area_finalize;

	obj_props[PROP_MODE] = g_param_spec_enum(
		"mode", "Mode", "Key Mode", SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_ALIGNMENT] = g_param_spec_enum(
		"alignment", "Alignment", "Key Alignment",
		SMTK_TYPE_KEY_ALIGNMENT, SMTK_KEY_ALIGNMENT_END,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_DRAW_BORDER] = g_param_spec_boolean(
		"draw-border", "Draw Border", "Draw Key Border", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_MARGIN_RATIO] = g_param_spec_double(
		"margin-ratio", "Margin Ratio", "Key Margin Ratio", 0, 10, 0.4,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	obj_props[PROP_TIMEOUT] = g_param_spec_int(
		"timeout", "Text Timeout", "Text Timeout", 0, 30000, 1000,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
	g_object_class_install_properties(object_class, N_PROPS, obj_props);
}

GtkWidget *smtk_keys_area_new(void)
{
	SmtkKeysArea *area = g_object_new(SMTK_TYPE_KEYS_AREA, "vexpand", true,
					  "hexpand", true, NULL);
	return GTK_WIDGET(area);
}

void smtk_keys_area_add_key(SmtkKeysArea *area, char key[])
{
	g_return_if_fail(area != NULL);

	g_debug("Adding key: %s.", key);
	g_mutex_lock(&area->keys_mutex);
	struct key_data *last = NULL;
	if (area->keys != NULL)
		last = area->keys->data;
	if (area->mode == SMTK_KEY_MODE_COMPACT && last != NULL &&
	    g_strcmp0(last->string, key) == 0) {
		++last->counter;
		g_free(key);
	} else {
		struct key_data *key_data = g_malloc(sizeof(*key_data));
		key_data->string = key;
		key_data->counter = 1;
		area->keys = g_slist_prepend(area->keys, key_data);
	}
	g_timer_start(area->timer);
	g_mutex_unlock(&area->keys_mutex);

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(area));
}
