#include <gtk/gtk.h>

#include "smtk-enum-types.h"
#include "smtk-keys-area.h"
#include "smtk-keys-mapper.h"

struct key_data {
	char *string;
	int counter;
};

static void free_key_data(void *data)
{
	g_autofree struct key_data *key_data = data;
	if (key_data == NULL)
		return;
	g_clear_pointer(&key_data->string, g_free);
}

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

static GParamSpec *props[N_PROPS] = { NULL };

static void set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeysArea *this = SMTK_KEYS_AREA(o);

	switch (prop) {
	case PROP_MODE:
		this->mode = g_value_get_enum(value);
		break;
	case PROP_ALIGNMENT:
		this->alignment = g_value_get_enum(value);
		// Trigger re-draw because alignment changed.
		gtk_widget_queue_draw(GTK_WIDGET(this));
		break;
	case PROP_DRAW_BORDER:
		this->draw_border = g_value_get_boolean(value);
		// Trigger re-draw because border changed.
		gtk_widget_queue_draw(GTK_WIDGET(this));
		break;
	case PROP_MARGIN_RATIO:
		this->margin_ratio = g_value_get_double(value);
		// Trigger re-draw because margin changed.
		gtk_widget_queue_draw(GTK_WIDGET(this));
		break;
	case PROP_TIMEOUT:
		this->timeout = g_value_get_int(value);
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
	SmtkKeysArea *this = SMTK_KEYS_AREA(o);

	switch (prop) {
	case PROP_MODE:
		g_value_set_enum(value, this->mode);
		break;
	case PROP_ALIGNMENT:
		g_value_set_enum(value, this->alignment);
		break;
	case PROP_DRAW_BORDER:
		g_value_set_boolean(value, this->draw_border);
		break;
	case PROP_MARGIN_RATIO:
		g_value_set_double(value, this->margin_ratio);
		break;
	case PROP_TIMEOUT:
		g_value_set_int(value, this->timeout);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static int calculate_width(SmtkKeysArea *this)
{
	int result = 0;

	g_mutex_lock(&this->keys_mutex);
	for (GSList *iter = this->keys; iter; iter = iter->next) {
		if (result > this->width)
			break;

		const struct key_data *key_data = iter->data;
		const char *string = key_data->string;
		g_autofree char *counter =
			g_strdup_printf("×%d", key_data->counter);

		PangoRectangle s_ink;
		pango_layout_set_text(this->string_layout, string, -1);
		pango_layout_get_pixel_extents(
			this->string_layout, &s_ink, NULL
		);
		result += (s_ink.width + this->padding * 2);

		if (key_data->counter > 1) {
			PangoRectangle c_ink;
			pango_layout_set_text(this->counter_layout, counter, -1);
			pango_layout_get_pixel_extents(
				this->counter_layout, &c_ink, NULL
			);
			result += (c_ink.width + this->padding * 2);
		}

		if (iter->next != NULL)
			result += this->margin;
	}
	g_mutex_unlock(&this->keys_mutex);

	return result;
}

static void
draw_key(SmtkKeysArea *this, cairo_t *cr, const struct key_data *key_data)
{
	const char *string = key_data->string;
	g_autofree char *counter = g_strdup_printf("×%d", key_data->counter);

	if (key_data->counter > 1)
		g_debug("Drawing key: %s%s.", string, counter);
	else
		g_debug("Drawing key: %s.", string);

	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

	// See <https://docs.gtk.org/Pango/method.Layout.set_text.html#parameters>.
	PangoRectangle s_ink;
	PangoRectangle s_logical;
	pango_layout_set_text(this->string_layout, string, -1);
	pango_layout_get_pixel_extents(this->string_layout, &s_ink, &s_logical);
	g_debug("Ink: x is %d, y is %d, width is %d, height is %d.",
		s_ink.x,
		s_ink.y,
		s_ink.width,
		s_ink.height);
	g_debug("Logical: x is %d, y is %d, width is %d, height is %d.",
		s_logical.x,
		s_logical.y,
		s_logical.width,
		s_logical.height);

	int border_height = this->string_height;
	int border_width = s_ink.width + this->padding * 2;
	int border_y = (this->height - this->string_height) / 2;
	int border_x = this->last_key_x - border_width;

	PangoRectangle c_ink;
	if (key_data->counter > 1) {
		pango_layout_set_text(this->counter_layout, counter, -1);
		pango_layout_get_pixel_extents(
			this->counter_layout, &c_ink, NULL
		);

		// Don't forget to leave space for counter.
		border_x -= (c_ink.width + this->padding * 2);
	}

	g_debug("Border: x is %d, y is %d, width is %d, height is %d.",
		border_x,
		border_y,
		border_width,
		border_height);

	if (this->draw_border) {
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
		const int x = border_x + this->padding - s_ink.x;
		const int y = border_y + border_height - this->padding -
			      s_ink.height - s_ink.y;
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, this->string_layout);
		// Draw border.
		cairo_set_line_width(cr, this->padding / 5.0);
		cairo_rectangle(
			cr, border_x, border_y, border_width, border_height
		);
		cairo_stroke(cr);
	} else {
		// When no border, just let Pango align texts so they will
		// sit on the baseline. But some icons are too large so if they
		// are aligned by baseline, they will draw outside area, so we
		// use ink rectangle for them to align glyphs by bottom.
		//
		// Finally we get a center-baseline or center-bottom alignment
		// so keys won't move horizontally when toggling borders.
		const int x = border_x + this->padding - s_ink.x;
		int y = border_y - s_logical.y;
		if (y + s_logical.height > this->height) {
			g_debug("%s will draw outside area if aligned by "
				"baseline, will be aligned by bottom.",
				string);
			y = border_y + border_height - this->padding -
			    s_ink.height - s_ink.y;
		}
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, this->string_layout);
	}

	if (key_data->counter > 1) {
		// We always use the ink rectangles to align counters because
		// they are all similiar and this is simple.
		const int x = border_x + border_width + this->padding - c_ink.x;
		const int y = border_y + border_height - c_ink.height - c_ink.y;
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, this->counter_layout);
	}

	this->last_key_x = border_x - this->margin;
}

static void
draw(GtkDrawingArea *drawing_area,
     cairo_t *cr,
     int width,
     int height,
     void *data)
{
	SmtkKeysArea *this = SMTK_KEYS_AREA(drawing_area);

	this->width = width;
	this->height = height;

	if (this->string_layout) {
		pango_cairo_update_layout(cr, this->string_layout);
	} else {
		this->string_layout = pango_cairo_create_layout(cr);
		pango_layout_set_ellipsize(
			this->string_layout, PANGO_ELLIPSIZE_NONE
		);
	}

	this->string_height = this->height * 0.8;
	const int string_font_size = this->string_height * 0.9;
	g_debug("String font size: %d.", string_font_size);
	pango_font_description_set_absolute_size(
		this->string_font, string_font_size * PANGO_SCALE
	);
	pango_layout_set_font_description(
		this->string_layout, this->string_font
	);

	if (this->counter_layout) {
		pango_cairo_update_layout(cr, this->counter_layout);
	} else {
		this->counter_layout = pango_cairo_create_layout(cr);
		pango_layout_set_ellipsize(
			this->counter_layout, PANGO_ELLIPSIZE_NONE
		);
	}

	this->counter_height = this->string_height * 0.5;
	const int counter_font_size = this->counter_height * 0.8;
	g_debug("Counter font size: %d.", string_font_size);
	pango_font_description_set_absolute_size(
		this->counter_font, counter_font_size * PANGO_SCALE
	);
	pango_layout_set_font_description(
		this->counter_layout, this->counter_font
	);

	this->margin = this->string_height * this->margin_ratio;
	this->padding = (this->string_height - string_font_size) / 2;

	// To align to center, we need to first calculate total width.
	if (this->alignment == SMTK_KEY_ALIGNMENT_END) {
		this->last_key_x = this->width;
	} else {
		const int keys_width = calculate_width(this);
		if (keys_width > this->width)
			this->last_key_x = this->width;
		else
			this->last_key_x =
				(this->width - keys_width) / 2 + keys_width;
	}

	g_mutex_lock(&this->keys_mutex);
	for (GSList *iter = this->keys; iter; iter = iter->next) {
		if (this->last_key_x < 0) {
			// We cannot handle next one, but it's OK to break
			// current one.
			GSList *unused = iter->next;
			iter->next = NULL;
			if (unused != NULL)
				g_clear_slist(&unused, free_key_data);
			break;
		}
		draw_key(this, cr, iter->data);
	}
	g_debug("Keys list length: %d", g_slist_length(this->keys));
	g_mutex_unlock(&this->keys_mutex);
}

// true and false are C99 _Bool, but GLib expects gboolean, which is C99 int.
static int trigger_redraw(void *data)
{
	SmtkKeysArea *this = data;

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(this));

	return 0;
}

static void *on_timeout(void *data)
{
	SmtkKeysArea *this = data;

	while (this->timer_running) {
		g_mutex_lock(&this->keys_mutex);
		int elapsed = g_timer_elapsed(this->timer, NULL) * 1000.0;
		g_mutex_unlock(&this->keys_mutex);

		if (this->timeout > 0 && elapsed > this->timeout) {
			g_debug("Timer triggered, clear keys.");
			g_mutex_lock(&this->keys_mutex);
			g_clear_slist(&this->keys, free_key_data);
			g_timer_start(this->timer);
			g_mutex_unlock(&this->keys_mutex);

			// Here is not UI thread so we need to kick an async
			// callback into GLib's main loop.
			g_timeout_add_full(
				G_PRIORITY_DEFAULT,
				0,
				trigger_redraw,
				g_object_ref(this),
				g_object_unref
			);
		}

		g_usleep(1000L);
	}

	return NULL;
}

static void constructed(GObject *o)
{
	SmtkKeysArea *this = SMTK_KEYS_AREA(o);

	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(this), draw, NULL, NULL);

	this->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(
		this->settings, "mode", this, "mode", G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"alignment",
		this,
		"alignment",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"draw-border",
		this,
		"draw-border",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"margin-ratio",
		this,
		"margin-ratio",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings, "timeout", this, "timeout", G_SETTINGS_BIND_GET
	);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->constructed(o);
}

static void dispose(GObject *o)
{
	SmtkKeysArea *this = SMTK_KEYS_AREA(o);

	g_clear_object(&this->settings);

	g_clear_object(&this->string_layout);
	g_clear_pointer(&this->string_font, pango_font_description_free);

	g_clear_object(&this->counter_layout);
	g_clear_pointer(&this->counter_font, pango_font_description_free);

	if (this->timer_thread != NULL) {
		this->timer_running = false;
		g_thread_join(this->timer_thread);
		this->timer_thread = NULL;
	}

	g_clear_pointer(&this->timer, g_timer_destroy);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->dispose(o);
}

static void finalize(GObject *o)
{
	SmtkKeysArea *this = SMTK_KEYS_AREA(o);

	g_clear_slist(&this->keys, free_key_data);

	G_OBJECT_CLASS(smtk_keys_area_parent_class)->finalize(o);
}

static void smtk_keys_area_class_init(SmtkKeysAreaClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);

	o_class->constructed = constructed;

	o_class->set_property = set_property;
	o_class->get_property = get_property;

	o_class->dispose = dispose;
	o_class->finalize = finalize;

	props[PROP_MODE] = g_param_spec_enum(
		"mode",
		"Mode",
		"Key Mode",
		SMTK_TYPE_KEY_MODE,
		SMTK_KEY_MODE_COMPOSED,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_ALIGNMENT] = g_param_spec_enum(
		"alignment",
		"Alignment",
		"Key Alignment",
		SMTK_TYPE_KEY_ALIGNMENT,
		SMTK_KEY_ALIGNMENT_END,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_DRAW_BORDER] = g_param_spec_boolean(
		"draw-border",
		"Draw Border",
		"Draw Key Border",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_MARGIN_RATIO] = g_param_spec_double(
		"margin-ratio",
		"Margin Ratio",
		"Key Margin Ratio",
		0,
		10,
		0.4,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_TIMEOUT] = g_param_spec_int(
		"timeout",
		"Text Timeout",
		"Text Timeout",
		0,
		30000,
		1000,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_keys_area_init(SmtkKeysArea *this)
{
	this->settings = NULL;
	this->width = 0;
	this->height = 0;

	this->string_layout = NULL;
	this->string_font = pango_font_description_new();
	pango_font_description_set_family(this->string_font, "monospace");
	this->string_height = 0;

	this->counter_layout = NULL;
	this->counter_font = pango_font_description_new();
	pango_font_description_set_family(this->counter_font, "monospace");
	this->counter_height = 0;

	this->keys = NULL;
	g_mutex_init(&this->keys_mutex);

	this->timer_running = true;
	this->timer = g_timer_new();

	// Just ignore timer thread error because it should work without timer.
	this->timer_thread = g_thread_try_new("timer", on_timeout, this, NULL);
}

GtkWidget *smtk_keys_area_new(void)
{
	SmtkKeysArea *this = g_object_new(
		SMTK_TYPE_KEYS_AREA, "vexpand", true, "hexpand", true, NULL
	);
	return GTK_WIDGET(this);
}

void smtk_keys_area_add_key(SmtkKeysArea *this, const char key[])
{
	g_return_if_fail(this != NULL);

	g_debug("Adding key: %s.", key);
	g_mutex_lock(&this->keys_mutex);
	struct key_data *last = NULL;
	if (this->keys != NULL)
		last = this->keys->data;
	if (this->mode == SMTK_KEY_MODE_COMPACT && last != NULL &&
	    g_strcmp0(last->string, key) == 0) {
		++last->counter;
	} else {
		struct key_data *key_data = g_malloc0(sizeof(*key_data));
		key_data->string = g_strdup(key);
		key_data->counter = 1;
		this->keys = g_slist_prepend(this->keys, key_data);
	}
	g_timer_start(this->timer);
	g_mutex_unlock(&this->keys_mutex);

	// Trigger re-draw because content changed.
	gtk_widget_queue_draw(GTK_WIDGET(this));
}
