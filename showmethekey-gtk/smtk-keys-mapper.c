#include <xkbcommon/xkbcommon.h>
#include <glib.h>
#include <gio/gio.h>

#include "smtk-enum-types.h"
#include "smtk-keys-mapper.h"
#include "smtk-event.h"
#include "smtk-keymap-list.h"

#define KEY_CODE_EV_TO_XKB(key_code) ((key_code) + 8)
#define XKB_KEY_SYM_NAME_LENGTH 64

// Adapt to virtual modifier changes in XKBcommon v1.8.
//
// See <https://github.com/xkbcommon/libxkbcommon/pull/759/files>.
#if defined(XKB_VMOD_NAME_SUPER)
#	define MOD_SUPER XKB_VMOD_NAME_SUPER
#else
#	define MOD_SUPER XKB_MOD_NAME_LOGO
#endif

#define MOD_CTRL XKB_MOD_NAME_CTRL

#if defined(XKB_VMOD_NAME_ALT)
#	define MOD_ALT XKB_VMOD_NAME_ALT
#else
#	define MOD_ALT XKB_MOD_NAME_ALT
#endif

#define MOD_SHIFT XKB_MOD_NAME_SHIFT

// Old XKBcommon does not define `XKB_MOD_NAME_MOD5`.
#if defined(XKB_VMOD_NAME_LEVEL3)
#	define MOD_ALTGR XKB_VMOD_NAME_LEVEL3
#elif defined(XKB_MOD_NAME_MOD5)
#	define MOD_ALTGR XKB_MOD_NAME_MOD5
#else
#	define MOD_ALTGR "Mod5"
#endif

struct _SmtkKeysMapper {
	GObject parent_instance;
	GSettings *settings;
	bool show_shift;
	bool hide_visible;
	char *layout;
	char *variant;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
	struct xkb_state *xkb_state_empty;
	GHashTable *xkb_mod_names;
	GHashTable *composed_replace_names;
	GHashTable *compact_replace_names;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysMapper, smtk_keys_mapper, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_SHOW_SHIFT,
	PROP_HIDE_VISIBLE,
	PROP_LAYOUT,
	PROP_VARIANT,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL };

// Prevent clang-format from adding space between minus.
// clang-format off
G_DEFINE_QUARK(smtk-keys-mapper-error-quark, smtk_keys_mapper_error)
// clang-format on

static bool refresh_keymap(SmtkKeysMapper *this)
{
	if (this->xkb_keymap != NULL)
		xkb_keymap_unref(this->xkb_keymap);

	struct xkb_rule_names names = {
		NULL,
		NULL,
		smtk_keymap_is_default(this->layout) ? NULL : this->layout,
		smtk_keymap_is_default(this->variant) ? NULL : this->variant,
		NULL
	};
	this->xkb_keymap = xkb_keymap_new_from_names(
		this->xkb_context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS
	);
	if (this->xkb_keymap == NULL) {
		g_set_error(
			&this->error,
			SMTK_KEYS_MAPPER_ERROR,
			SMTK_KEYS_MAPPER_ERROR_XKB_KEYMAP,
			"Failed to create XKB keymap."
		);
		return false;
	}

	if (this->xkb_state != NULL)
		xkb_state_unref(this->xkb_state);
	if (this->xkb_state_empty != NULL)
		xkb_state_unref(this->xkb_state_empty);

	this->xkb_state = xkb_state_new(this->xkb_keymap);
	if (this->xkb_state == NULL) {
		g_set_error(
			&this->error,
			SMTK_KEYS_MAPPER_ERROR,
			SMTK_KEYS_MAPPER_ERROR_XKB_STATE,
			"Failed to create XKB state."
		);
		return false;
	}
	this->xkb_state_empty = xkb_state_new(this->xkb_keymap);
	if (this->xkb_state_empty == NULL) {
		g_set_error(
			&this->error,
			SMTK_KEYS_MAPPER_ERROR,
			SMTK_KEYS_MAPPER_ERROR_XKB_STATE,
			"Failed to create empty XKB state."
		);
		return false;
	}

	return true;
}

static void set_layout(SmtkKeysMapper *this, const char *layout)
{
	g_clear_pointer(&this->layout, g_free);
	this->layout = g_strdup(layout);

	// If we first change layout then change variant, we may get error
	// between two function calls, it is safe to ignore it.
	refresh_keymap(this);
}

static void set_variant(SmtkKeysMapper *this, const char *variant)
{
	g_clear_pointer(&this->variant, g_free);
	this->variant = g_strdup(variant);

	refresh_keymap(this);
}

static void set_property(
	GObject *o,
	unsigned int prop,
	const GValue *value,
	GParamSpec *pspec
)
{
	SmtkKeysMapper *this = SMTK_KEYS_MAPPER(o);

	switch (prop) {
	case PROP_SHOW_SHIFT:
		this->show_shift = g_value_get_boolean(value);
		break;
	case PROP_HIDE_VISIBLE:
		this->hide_visible = g_value_get_boolean(value);
		break;
	case PROP_LAYOUT:
		set_layout(this, g_value_get_string(value));
		break;
	case PROP_VARIANT:
		set_variant(this, g_value_get_string(value));
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
	SmtkKeysMapper *this = SMTK_KEYS_MAPPER(o);

	switch (prop) {
	case PROP_SHOW_SHIFT:
		g_value_set_boolean(value, this->show_shift);
		break;
	case PROP_HIDE_VISIBLE:
		g_value_set_boolean(value, this->hide_visible);
		break;
	case PROP_LAYOUT:
		g_value_set_string(value, this->layout);
		break;
	case PROP_VARIANT:
		g_value_set_string(value, this->variant);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(o, prop, pspec);
		break;
	}
}

static void constructed(GObject *object)
{
	SmtkKeysMapper *this = SMTK_KEYS_MAPPER(object);

	this->settings = g_settings_new("one.alynx.showmethekey");
	g_settings_bind(
		this->settings,
		"show-shift",
		this,
		"show-shift",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings,
		"hide-visible",
		this,
		"hide-visible",
		G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings, "layout", this, "layout", G_SETTINGS_BIND_GET
	);
	g_settings_bind(
		this->settings, "variant", this, "variant", G_SETTINGS_BIND_GET
	);

	refresh_keymap(this);

	G_OBJECT_CLASS(smtk_keys_mapper_parent_class)->constructed(object);
}

static void dispose(GObject *object)
{
	SmtkKeysMapper *this = SMTK_KEYS_MAPPER(object);

	g_clear_object(&this->settings);

	g_clear_pointer(&this->xkb_state, xkb_state_unref);
	g_clear_pointer(&this->xkb_state_empty, xkb_state_unref);
	g_clear_pointer(&this->xkb_keymap, xkb_keymap_unref);
	g_clear_pointer(&this->xkb_context, xkb_context_unref);

	g_clear_pointer(&this->xkb_mod_names, g_hash_table_unref);
	g_clear_pointer(&this->composed_replace_names, g_hash_table_unref);

	G_OBJECT_CLASS(smtk_keys_mapper_parent_class)->dispose(object);
}

static void finalize(GObject *object)
{
	SmtkKeysMapper *this = SMTK_KEYS_MAPPER(object);

	g_clear_pointer(&this->layout, g_free);
	g_clear_pointer(&this->variant, g_free);

	G_OBJECT_CLASS(smtk_keys_mapper_parent_class)->finalize(object);
}

static void smtk_keys_mapper_class_init(SmtkKeysMapperClass *klass)
{
	GObjectClass *o_class = G_OBJECT_CLASS(klass);

	o_class->set_property = set_property;
	o_class->get_property = get_property;

	o_class->constructed = constructed;

	o_class->dispose = dispose;
	o_class->finalize = finalize;

	props[PROP_SHOW_SHIFT] = g_param_spec_boolean(
		"show-shift",
		"Show Shift",
		"Show Shift Separately",
		true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_HIDE_VISIBLE] = g_param_spec_boolean(
		"hide-visible",
		"Hide Visible",
		"Hide Visible Keys",
		false,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_LAYOUT] = g_param_spec_string(
		"layout",
		"Layout",
		"Keymap Layout",
		NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);
	props[PROP_VARIANT] = g_param_spec_string(
		"variant",
		"Variant",
		"Keymap Variant",
		NULL,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE
	);

	g_object_class_install_properties(o_class, N_PROPS, props);
}

static void smtk_keys_mapper_init(SmtkKeysMapper *this)
{
	this->error = NULL;
	this->settings = NULL;
	this->layout = NULL;
	this->variant = NULL;

	this->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (this->xkb_context == NULL) {
		g_set_error(
			&this->error,
			SMTK_KEYS_MAPPER_ERROR,
			SMTK_KEYS_MAPPER_ERROR_XKB_CONTEXT,
			"Failed to create XKB context."
		);
		return;
	}

	// GHashTable only keeps reference, so we manully copy string and set
	// g_free() for releasing them.
	// We only use key in this GHashTable,
	// so don't pass a value free function to it!
	this->xkb_mod_names =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_add(this->xkb_mod_names, g_strdup("Super_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Super_R"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Control_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Control_R"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Alt_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Alt_R"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Shift_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Shift_R"));
	// See <https://github.com/AlynxZhou/showmethekey/issues/83>.
	// A more common name is AltGr, and it is a modifier.
	g_hash_table_add(this->xkb_mod_names, g_strdup("ISO_Level3_Shift"));
	// I believe nowadays there is no keyboard that has those keys,
	// Shift+Alt will get Meta_L and Meta_R, but why not show Shift+Alt
	// then? I am a Emacs user but I even don't know how to input Hyper, and
	// in Emacs Meta is just Alt. I will ignore those virtual keys unless
	// you show me some keyboards with real keys for Meta and Hyper.
	g_hash_table_add(this->xkb_mod_names, g_strdup("Meta_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Meta_R"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Hyper_L"));
	g_hash_table_add(this->xkb_mod_names, g_strdup("Hyper_R"));

	// Some key names I don't like, replace them. There is no need to
	// replace modifiers here because they are handled differently.
	this->composed_replace_names =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	// It should be safe to add "Shift+" here, if you want to make some
	// Shift modified key raw. For example ISO_Left_Tab here.
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("ISO_Left_Tab"),
		g_strdup("Shift+Tab")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("XF86AudioMute"),
		g_strdup("MuteToggle")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("XF86AudioLowerVolume"),
		g_strdup("VolumnDown")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("XF86AudioRaiseVolume"),
		g_strdup("VolumnUp")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("XF86MonBrightnessDown"),
		g_strdup("BrightnessDown")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("XF86MonBrightnessUp"),
		g_strdup("BrightnessUp")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Num_Lock"),
		g_strdup("NumLock")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("asciitilde"),
		g_strdup("~")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("grave"), g_strdup("`")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("exclam"), g_strdup("!")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("at"), g_strdup("@")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("numbersign"),
		g_strdup("#")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("dollar"), g_strdup("$")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("percent"), g_strdup("%")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("asciicircum"),
		g_strdup("^")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("ampersand"),
		g_strdup("&")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("asterisk"),
		g_strdup("*")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("parenleft"),
		g_strdup("(")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("parenright"),
		g_strdup(")")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("minus"), g_strdup("-")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("underscore"),
		g_strdup("_")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("equal"), g_strdup("=")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("plus"), g_strdup("+")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("bracketleft"),
		g_strdup("[")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("braceleft"),
		g_strdup("{")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("bracketright"),
		g_strdup("]")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("braceright"),
		g_strdup("}")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("backslash"),
		g_strdup("\\")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("bar"), g_strdup("|")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Caps_Lock"),
		g_strdup("CapsLock")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("semicolon"),
		g_strdup(";")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("colon"), g_strdup(":")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("apostrophe"),
		g_strdup("'")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("quotedbl"),
		g_strdup("\"")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Return"),
		g_strdup("Enter")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Escape"),
		g_strdup("Escape")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("comma"), g_strdup(",")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("less"), g_strdup("<")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("period"), g_strdup(".")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("greater"), g_strdup(">")
	);
	g_hash_table_insert(
		this->composed_replace_names, g_strdup("slash"), g_strdup("/")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("question"),
		g_strdup("?")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("space"),
		g_strdup("Space")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Print"),
		g_strdup("PrintScreen")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Sys_Req"),
		g_strdup("SysReq")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Scroll_Lock"),
		g_strdup("ScrollLock")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Prior"),
		g_strdup("PageUp")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("Next"),
		g_strdup("PageDown")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("BTN_LEFT"),
		g_strdup("MouseLeft")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("BTN_RIGHT"),
		g_strdup("MouseRight")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("BTN_MIDDLE"),
		g_strdup("MouseMiddle")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("BTN_EXTRA"),
		g_strdup("MouseForward")
	);
	g_hash_table_insert(
		this->composed_replace_names,
		g_strdup("BTN_SIDE"),
		g_strdup("MouseBack")
	);

	// For simple mode, we use shorter strings and icons.
	//
	// Icons are chosen based on @arielherself's idea.
	//
	// See <https://github.com/AlynxZhou/showmethekey/pull/43/files>.
	this->compact_replace_names =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("ISO_Left_Tab"),
		g_strdup("⭰")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Meta_L"),
		g_strdup("Meta")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Meta_R"),
		g_strdup("Meta")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("XF86AudioMute"),
		g_strdup("🔇")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("XF86AudioLowerVolume"),
		g_strdup("🔈")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("XF86AudioRaiseVolume"),
		g_strdup("🔊")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("XF86MonBrightnessDown"),
		g_strdup("🔅")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("XF86MonBrightnessUp"),
		g_strdup("🔆")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Num_Lock"), g_strdup("⇭")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("asciitilde"),
		g_strdup("~")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("grave"), g_strdup("`")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("exclam"), g_strdup("!")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("at"), g_strdup("@")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("numbersign"),
		g_strdup("#")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("dollar"), g_strdup("$")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("percent"), g_strdup("%")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("asciicircum"),
		g_strdup("^")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("ampersand"),
		g_strdup("&")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("asterisk"), g_strdup("*")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("parenleft"),
		g_strdup("(")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("parenright"),
		g_strdup(")")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("minus"), g_strdup("-")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("underscore"),
		g_strdup("_")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("equal"), g_strdup("=")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("plus"), g_strdup("+")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("bracketleft"),
		g_strdup("[")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("braceleft"),
		g_strdup("{")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("bracketright"),
		g_strdup("]")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("braceright"),
		g_strdup("}")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("backslash"),
		g_strdup("\\")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("bar"), g_strdup("|")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Caps_Lock"),
		g_strdup("⇪")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("semicolon"),
		g_strdup(";")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("colon"), g_strdup(":")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("apostrophe"),
		g_strdup("'")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("quotedbl"),
		g_strdup("\"")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Return"), g_strdup("⏎")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Escape"), g_strdup("Esc")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("comma"), g_strdup(",")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("less"), g_strdup("<")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("period"), g_strdup(".")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("greater"), g_strdup(">")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("slash"), g_strdup("/")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("question"), g_strdup("?")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("space"), g_strdup("⎵")
	);
	// The UTF-8 Print Screen Symbol is too large for some fonts.
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Print"), g_strdup("⎙")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Sys_Req"),
		g_strdup("SysReq")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Scroll_Lock"),
		g_strdup("⇳")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Prior"),
		g_strdup("PageUp")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("Next"),
		g_strdup("PageDown")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BTN_LEFT"),
		g_strdup("🖰↖")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BTN_RIGHT"),
		g_strdup("🖰↗")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BTN_MIDDLE"),
		g_strdup("🖰↕")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BTN_EXTRA"),
		g_strdup("🖰F")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BTN_SIDE"),
		g_strdup("🖰B")
	);
	g_hash_table_insert(
		this->compact_replace_names,
		g_strdup("BackSpace"),
		g_strdup("⌫")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Delete"), g_strdup("⌦")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Tab"), g_strdup("⇥")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Left"), g_strdup("←")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Right"), g_strdup("→")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Up"), g_strdup("↑")
	);
	g_hash_table_insert(
		this->compact_replace_names, g_strdup("Down"), g_strdup("↓")
	);
}

SmtkKeysMapper *smtk_keys_mapper_new(GError **error)
{
	SmtkKeysMapper *this = g_object_new(SMTK_TYPE_KEYS_MAPPER, NULL);

	if (this->error != NULL) {
		g_propagate_error(error, this->error);
		g_object_unref(this);
		return NULL;
	}

	return this;
}

char *smtk_keys_mapper_get_raw(SmtkKeysMapper *this, SmtkEvent *event)
{
	g_return_val_if_fail(this != NULL, NULL);
	g_return_val_if_fail(event != NULL, NULL);

	// XKBcommon only handles keyboard.
	if (event->type == SMTK_EVENT_TYPE_KEYBOARD_KEY) {
		xkb_keycode_t xkb_key_code =
			KEY_CODE_EV_TO_XKB(event->key_code);
		SmtkEventState state = event->state;
		xkb_state_update_key(
			this->xkb_state,
			xkb_key_code,
			state == SMTK_EVENT_STATE_PRESSED ? XKB_KEY_DOWN :
							    XKB_KEY_UP
		);
	}
	return g_strdup(event->key_name);
}

// Shift is a little bit complex, it can be consumed by capitalization
// transformation, so we check it here. This prevents text like Shift+! but
// allows text like Shift+PrintScreen. However, if user sets `show_shift`,
// always append it even consumed.
//
// AltGr == ISO_Level3_Shift == XKB_MOD_NAME_MOD5, handle it like Shift.
// See <https://xkbcommon.org/doc/current/keymap-text-format-v1.html#terminology>.
static bool check_show(
	SmtkKeysMapper *this,
	const char *modifier,
	xkb_keycode_t xkb_key_code
)
{
	return xkb_state_mod_name_is_active(
		       this->xkb_state, modifier, XKB_STATE_MODS_EFFECTIVE
	       ) > 0 &&
	       (this->show_shift ||
		!xkb_state_mod_index_is_consumed(
			this->xkb_state,
			xkb_key_code,
			xkb_keymap_mod_get_index(this->xkb_keymap, modifier)
		));
}

static char *concat_key(
	SmtkKeysMapper *this,
	SmtkKeyMode mode,
	const char *main_key,
	xkb_keycode_t xkb_key_code
)
{
	// Use a GString for easior mods concat.
	GString *buffer = g_string_new(NULL);
	// I'd like to call it "Super".
	if (xkb_state_mod_name_is_active(
		    this->xkb_state, MOD_SUPER, XKB_STATE_MODS_EFFECTIVE
	    ) > 0)
		g_string_append(
			buffer, mode == SMTK_KEY_MODE_COMPACT ? "⌘" : "Super+"
		);
	if (xkb_state_mod_name_is_active(
		    this->xkb_state, MOD_CTRL, XKB_STATE_MODS_EFFECTIVE
	    ) > 0)
		g_string_append(
			buffer, mode == SMTK_KEY_MODE_COMPACT ? "⌃" : "Ctrl+"
		);
	if (xkb_state_mod_name_is_active(
		    this->xkb_state, MOD_ALT, XKB_STATE_MODS_EFFECTIVE
	    ) > 0)
		g_string_append(
			buffer, mode == SMTK_KEY_MODE_COMPACT ? "⌥" : "Alt+"
		);
	if (check_show(this, MOD_SHIFT, xkb_key_code))
		g_string_append(
			buffer, mode == SMTK_KEY_MODE_COMPACT ? "⇧" : "Shift+"
		);
	if (check_show(this, MOD_ALTGR, xkb_key_code))
		g_string_append(buffer, "AltGr+");
	g_string_append(buffer, main_key);
	return g_string_free_and_steal(buffer);
}

static bool is_visible_key(struct xkb_state *xkb_state)
{
	// Ideally we should check whether a key is insertable to editor, but
	// xkbcommon does not provide such a function, and it is hard to
	// implement by ourselves, because keysyms are not continuous and cannot
	// be simply filtered out by range, so we only check modifiers. Shift
	// always generates insertable keys so don't check it here.
	g_debug("Super: %d, Ctrl: %d, Alt: %d.",
		xkb_state_mod_name_is_active(
			xkb_state, MOD_SUPER, XKB_STATE_MODS_EFFECTIVE
		),
		xkb_state_mod_name_is_active(
			xkb_state, MOD_CTRL, XKB_STATE_MODS_EFFECTIVE
		),
		xkb_state_mod_name_is_active(
			xkb_state, MOD_ALT, XKB_STATE_MODS_EFFECTIVE
		));
	return !(
		xkb_state_mod_name_is_active(
			xkb_state, MOD_SUPER, XKB_STATE_MODS_EFFECTIVE
		) ||
		xkb_state_mod_name_is_active(
			xkb_state, MOD_CTRL, XKB_STATE_MODS_EFFECTIVE
		) ||
		xkb_state_mod_name_is_active(
			xkb_state, MOD_ALT, XKB_STATE_MODS_EFFECTIVE
		)
	);
}

static char *get_key(SmtkKeysMapper *this, SmtkKeyMode mode, SmtkEvent *event)
{
	g_autofree char *main_key = NULL;
	// We put xkb_key_code here because the Shift detection use it.
	// Though Xkbcommon don't handle mouse button state, we can still
	// convert evdev key code to xkb key code.
	xkb_keycode_t xkb_key_code = KEY_CODE_EV_TO_XKB(event->key_code);
	// Xkbcommon only handle keyboards,
	// so we use libinput key name for mouse button.
	if (event->type == SMTK_EVENT_TYPE_KEYBOARD_KEY) {
		SmtkEventState state = event->state;
		xkb_state_update_key(
			this->xkb_state,
			xkb_key_code,
			state == SMTK_EVENT_STATE_PRESSED ? XKB_KEY_DOWN :
							    XKB_KEY_UP
		);
		xkb_keysym_t xkb_key_sym = xkb_state_key_get_one_sym(
			this->xkb_state, xkb_key_code
		);
		// Always get the unshifted key if show shift.
		if (check_show(this, MOD_SHIFT, xkb_key_code) ||
		    check_show(this, MOD_ALTGR, xkb_key_code))
			xkb_key_sym = xkb_state_key_get_one_sym(
				this->xkb_state_empty, xkb_key_code
			);
		if (this->hide_visible && is_visible_key(this->xkb_state)) {
			g_debug("Hide visible key.");
			return NULL;
		}
		main_key = g_malloc0(XKB_KEY_SYM_NAME_LENGTH);
		xkb_keysym_get_name(
			xkb_key_sym, main_key, XKB_KEY_SYM_NAME_LENGTH
		);
	} else {
		main_key = g_strdup(event->key_name);
	}
	// Just ignore mods so we can prevent text like mod+mod.
	if (g_hash_table_contains(this->xkb_mod_names, main_key)) {
		g_debug("Ignore pure mod.");
		return NULL;
	}
	const char *replace_name = g_hash_table_lookup(
		mode == SMTK_KEY_MODE_COMPACT ? this->compact_replace_names :
						this->composed_replace_names,
		main_key
	);
	if (replace_name != NULL) {
		g_clear_pointer(&main_key, g_free);
		// Copy from the string reference in GHashTable,
		// so we can use g_free() after append it into GString.
		main_key = g_strdup(replace_name);
	}
	return concat_key(this, mode, main_key, xkb_key_code);
}

char *smtk_keys_mapper_get_composed(SmtkKeysMapper *this, SmtkEvent *event)
{
	g_return_val_if_fail(this != NULL, NULL);
	g_return_val_if_fail(event != NULL, NULL);

	return get_key(this, SMTK_KEY_MODE_COMPOSED, event);
}

char *smtk_keys_mapper_get_compact(SmtkKeysMapper *this, SmtkEvent *event)
{
	g_return_val_if_fail(this != NULL, NULL);
	g_return_val_if_fail(event != NULL, NULL);

	return get_key(this, SMTK_KEY_MODE_COMPACT, event);
}
