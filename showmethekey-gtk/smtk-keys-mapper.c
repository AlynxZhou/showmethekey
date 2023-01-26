#include <xkbcommon/xkbcommon.h>

#include "smtk.h"
#include "smtk-keys-mapper.h"
#include "smtk-event.h"

#define KEY_CODE_EV_TO_XKB(key_code) ((key_code) + 8)
#define XKB_KEY_SYM_NAME_LENGTH 64

struct _SmtkKeysMapper {
	GObject parent_instance;
	bool show_shift;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
	GHashTable *xkb_mod_names;
	GHashTable *xkb_replace_names;
	GError *error;
};
G_DEFINE_TYPE(SmtkKeysMapper, smtk_keys_mapper, G_TYPE_OBJECT)

enum { PROP_0, PROP_SHOW_SHIFT, N_PROPERTIES };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

// Prevent clang-format from adding space between minus.
// clang-format off
G_DEFINE_QUARK(smtk-keys-mapper-error-quark, smtk_keys_mapper_error)
// clang-format on

static void smtk_keys_mapper_set_property(GObject *object,
					  unsigned int property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	SmtkKeysMapper *mapper = SMTK_KEYS_MAPPER(object);

	switch (property_id) {
	case PROP_SHOW_SHIFT:
		mapper->show_shift = g_value_get_boolean(value);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_mapper_get_property(GObject *object,
					  unsigned int property_id,
					  GValue *value, GParamSpec *pspec)
{
	SmtkKeysMapper *mapper = SMTK_KEYS_MAPPER(object);

	switch (property_id) {
	case PROP_SHOW_SHIFT:
		g_value_set_boolean(value, mapper->show_shift);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void smtk_keys_mapper_init(SmtkKeysMapper *mapper)
{
	mapper->error = NULL;
	mapper->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (mapper->xkb_context == NULL) {
		g_set_error(&mapper->error, SMTK_KEYS_MAPPER_ERROR,
			    SMTK_KEYS_MAPPER_ERROR_XKB_CONTEXT,
			    "Failed to create XKB context.");
		return;
	}
	mapper->xkb_keymap = xkb_keymap_new_from_names(
		mapper->xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (mapper->xkb_keymap == NULL) {
		g_set_error(&mapper->error, SMTK_KEYS_MAPPER_ERROR,
			    SMTK_KEYS_MAPPER_ERROR_XKB_KEYMAP,
			    "Failed to create XKB keymap.");
		return;
	}
	mapper->xkb_state = xkb_state_new(mapper->xkb_keymap);
	if (mapper->xkb_state == NULL) {
		g_set_error(&mapper->error, SMTK_KEYS_MAPPER_ERROR,
			    SMTK_KEYS_MAPPER_ERROR_XKB_STATE,
			    "Failed to create XKB state.");
		return;
	}

	// GHashTable only keeps reference, so we manully copy string and set
	// g_free() for releasing them.
	// We only use key in this GHashTable,
	// so don't pass a value free function to it!
	mapper->xkb_mod_names =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Super_L"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Super_R"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Control_L"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Control_R"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Alt_L"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Alt_R"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Shift_L"));
	g_hash_table_add(mapper->xkb_mod_names, g_strdup("Shift_R"));

	// Some key names I don't like, replace them.
	mapper->xkb_replace_names =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	// It should be safe to add "Shift+" here, if you want to make some
	// Shift modified key raw. For example ISO_Left_Tab here.
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("ISO_Left_Tab"),
			    g_strdup("Shift+Tab"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Meta_L"),
			    g_strdup("Meta"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Meta_R"),
			    g_strdup("Meta"));
	g_hash_table_insert(mapper->xkb_replace_names,
			    g_strdup("XF86AudioMute"), g_strdup("MuteToggle"));
	g_hash_table_insert(mapper->xkb_replace_names,
			    g_strdup("XF86AudioLowerVolume"),
			    g_strdup("VolumnDown"));
	g_hash_table_insert(mapper->xkb_replace_names,
			    g_strdup("XF86AudioRaiseVolume"),
			    g_strdup("VolumnUp"));
	g_hash_table_insert(mapper->xkb_replace_names,
			    g_strdup("XF86MonBrightnessDown"),
			    g_strdup("BrightnessDown"));
	g_hash_table_insert(mapper->xkb_replace_names,
			    g_strdup("XF86MonBrightnessUp"),
			    g_strdup("BrightnessUp"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Num_Lock"),
			    g_strdup("NumLock"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("asciitilde"),
			    g_strdup("~"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("grave"),
			    g_strdup("`"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("exclam"),
			    g_strdup("!"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("at"),
			    g_strdup("@"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("numbersign"),
			    g_strdup("#"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("dollar"),
			    g_strdup("$"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("percent"),
			    g_strdup("%"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("asciicircum"),
			    g_strdup("^"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("ampersand"),
			    g_strdup("&"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("asterisk"),
			    g_strdup("*"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("parenleft"),
			    g_strdup("("));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("parenright"),
			    g_strdup(")"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("minus"),
			    g_strdup("-"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("underscore"),
			    g_strdup("_"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("equal"),
			    g_strdup("="));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("plus"),
			    g_strdup("+"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("bracketleft"),
			    g_strdup("["));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("braceleft"),
			    g_strdup("{"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("bracketright"),
			    g_strdup("]"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("braceright"),
			    g_strdup("}"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("backslash"),
			    g_strdup("\\"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("bar"),
			    g_strdup("|"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Caps_Lock"),
			    g_strdup("CapsLock"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("semicolon"),
			    g_strdup(";"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("colon"),
			    g_strdup(":"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("apostrophe"),
			    g_strdup("'"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("quotedbl"),
			    g_strdup("\""));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Return"),
			    g_strdup("Enter"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Escape"),
			    g_strdup("Escape"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("comma"),
			    g_strdup(","));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("less"),
			    g_strdup("<"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("period"),
			    g_strdup("."));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("greater"),
			    g_strdup(">"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("slash"),
			    g_strdup("/"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("question"),
			    g_strdup("?"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("space"),
			    g_strdup("Space"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Print"),
			    g_strdup("PrintScreen"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Sys_Req"),
			    g_strdup("SysReq"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Scroll_Lock"),
			    g_strdup("ScrollLock"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Prior"),
			    g_strdup("PageUp"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Next"),
			    g_strdup("PageDown"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("BTN_LEFT"),
			    g_strdup("MouseLeft"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("BTN_RIGHT"),
			    g_strdup("MouseRight"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("BTN_MIDDLE"),
			    g_strdup("MouseMiddle"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("BTN_EXTRA"),
			    g_strdup("MouseForward"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("BTN_SIDE"),
			    g_strdup("MouseBack"));
	g_hash_table_insert(mapper->xkb_replace_names, g_strdup("Num_Lock"),
			    g_strdup("NumLock"));
}

static void smtk_keys_mapper_dispose(GObject *object)
{
	SmtkKeysMapper *mapper = SMTK_KEYS_MAPPER(object);

	g_clear_pointer(&mapper->xkb_state, xkb_state_unref);
	g_clear_pointer(&mapper->xkb_keymap, xkb_keymap_unref);
	g_clear_pointer(&mapper->xkb_context, xkb_context_unref);

	g_clear_pointer(&mapper->xkb_mod_names, g_hash_table_destroy);
	g_clear_pointer(&mapper->xkb_replace_names, g_hash_table_destroy);

	G_OBJECT_CLASS(smtk_keys_mapper_parent_class)->dispose(object);
}

static void smtk_keys_mapper_class_init(SmtkKeysMapperClass *mapper_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(mapper_class);

	object_class->set_property = smtk_keys_mapper_set_property;
	object_class->get_property = smtk_keys_mapper_get_property;

	object_class->dispose = smtk_keys_mapper_dispose;

	obj_properties[PROP_SHOW_SHIFT] = g_param_spec_boolean(
		"show-shift", "Show Shift", "Show Shift Separately", true,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES,
					  obj_properties);
}

SmtkKeysMapper *smtk_keys_mapper_new(bool show_shift, GError **error)
{
	SmtkKeysMapper *mapper = g_object_new(SMTK_TYPE_KEYS_MAPPER,
					      "show-shift", show_shift, NULL);

	if (mapper->error != NULL) {
		g_propagate_error(error, mapper->error);
		g_object_unref(mapper);
		return NULL;
	}

	return mapper;
}

char *smtk_keys_mapper_get_raw(SmtkKeysMapper *mapper, SmtkEvent *event)
{
	g_return_val_if_fail(mapper != NULL, NULL);
	g_return_val_if_fail(event != NULL, NULL);

	// XKBcommon only handles keyboard.
	if (smtk_event_get_event_type(event) == SMTK_EVENT_TYPE_KEYBOARD_KEY) {
		xkb_keycode_t xkb_key_code =
			KEY_CODE_EV_TO_XKB(smtk_event_get_key_code(event));
		SmtkEventState event_state = smtk_event_get_event_state(event);
		xkb_state_update_key(mapper->xkb_state, xkb_key_code,
				     event_state == SMTK_EVENT_STATE_PRESSED ?
					     XKB_KEY_DOWN :
					     XKB_KEY_UP);
	}
	return g_strdup(smtk_event_get_key_name(event));
}

char *smtk_keys_mapper_get_composed(SmtkKeysMapper *mapper, SmtkEvent *event)
{
	g_return_val_if_fail(mapper != NULL, NULL);
	g_return_val_if_fail(event != NULL, NULL);

	char *main_key = NULL;
	// We put xkb_key_code here because the Shift detection use it.
	// Though Xkbcommon don't handle mouse button state, we can still
	// convert evdev key code to xkb key code.
	xkb_keycode_t xkb_key_code =
		KEY_CODE_EV_TO_XKB(smtk_event_get_key_code(event));
	// Xkbcommon only handle keyboards,
	// so we use libinput key name for mouse button.
	if (smtk_event_get_event_type(event) == SMTK_EVENT_TYPE_KEYBOARD_KEY) {
		SmtkEventState event_state = smtk_event_get_event_state(event);
		xkb_state_update_key(mapper->xkb_state, xkb_key_code,
				     event_state == SMTK_EVENT_STATE_PRESSED ?
					     XKB_KEY_DOWN :
					     XKB_KEY_UP);
		xkb_keysym_t xkb_key_sym = xkb_state_key_get_one_sym(
			mapper->xkb_state, xkb_key_code);
		main_key = g_malloc(XKB_KEY_SYM_NAME_LENGTH);
		xkb_keysym_get_name(xkb_key_sym, main_key,
				    XKB_KEY_SYM_NAME_LENGTH);
	} else {
		main_key = g_strdup(smtk_event_get_key_name(event));
	}
	// Just ignore mods so we can prevent text like mod+mod.
	if (g_hash_table_contains(mapper->xkb_mod_names, main_key)) {
		g_free(main_key);
		return NULL;
	}
	const char *replace_name =
		g_hash_table_lookup(mapper->xkb_replace_names, main_key);
	if (replace_name != NULL) {
		g_free(main_key);
		// Copy from the string reference in GHashTable,
		// so we can use g_free() after append it into GString.
		main_key = g_strdup(replace_name);
	}
	// Use a GString for easior mods concat.
	GString *buffer = g_string_new(NULL);
	// I'd like to call it "Super".
	if (xkb_state_mod_name_is_active(mapper->xkb_state, XKB_MOD_NAME_LOGO,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		g_string_append(buffer, "Super+");
	if (xkb_state_mod_name_is_active(mapper->xkb_state, XKB_MOD_NAME_CTRL,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		g_string_append(buffer, "Ctrl+");
	// Shift+Alt will get Meta_L and Meta_R,
	// and we should not add Alt for it.
	// I think Meta should be a modifier, but Xkbcommon does not.
	// Sounds like a bug.
	if (xkb_state_mod_name_is_active(mapper->xkb_state, XKB_MOD_NAME_ALT,
					 XKB_STATE_MODS_EFFECTIVE) > 0 &&
	    strcmp(main_key, "Meta") != 0)
		g_string_append(buffer, "Alt+");
	if (xkb_state_mod_name_is_active(mapper->xkb_state, XKB_MOD_NAME_SHIFT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		// Shift is a little bit complex, it can be consumed by
		// capitalization transformation, so we check it here. This
		// prevents text like Shift+! but allows text like
		// Shift+PrintScreen. However, if user sets `show_shift`, always
		// append it even consumed.
		if (mapper->show_shift ||
		    !xkb_state_mod_index_is_consumed(
			    mapper->xkb_state, xkb_key_code,
			    xkb_keymap_mod_get_index(mapper->xkb_keymap,
						     XKB_MOD_NAME_SHIFT)))
			g_string_append(buffer, "Shift+");
	g_string_append(buffer, main_key);
	g_free(main_key);
	return g_string_free(buffer, FALSE);
}

void smtk_keys_mapper_set_show_shift(SmtkKeysMapper *mapper, bool show_shift)
{
	g_return_if_fail(mapper != NULL);

	g_object_set(mapper, "show-shift", show_shift, NULL);
}
