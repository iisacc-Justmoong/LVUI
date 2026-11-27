#pragma once

#include "CompilerBridge.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define LVUI_EDITOR_NOEXCEPT noexcept
#else
#define LVUI_EDITOR_NOEXCEPT
#endif

enum { LVUI_EDITOR_MENU_PARAMETERS, LVUI_EDITOR_PREVIEW, LVUI_EDITOR_MASK_REFINEMENT,
       LVUI_EDITOR_LAYER_CONTENT, LVUI_EDITOR_FILTER_GALLERY, LVUI_EDITOR_COLOR_PROFILE,
       LVUI_EDITOR_SOFT_PROOF, LVUI_EDITOR_TIMELINE, LVUI_EDITOR_EXPORT_FORMAT };
enum { LVUI_EDITOR_FIELD_TEXT, LVUI_EDITOR_FIELD_INTEGER, LVUI_EDITOR_FIELD_REAL,
       LVUI_EDITOR_FIELD_BOOLEAN, LVUI_EDITOR_FIELD_CHOICE };
enum { LVUI_EDITOR_VALUE_STRING, LVUI_EDITOR_VALUE_INTEGER, LVUI_EDITOR_VALUE_REAL, LVUI_EDITOR_VALUE_BOOLEAN };
enum { LVUI_EDITOR_MENU_ACTION, LVUI_EDITOR_MENU_SUBMENU, LVUI_EDITOR_MENU_DIVIDER };
enum { LVUI_EDITOR_FRESH, LVUI_EDITOR_EDITING, LVUI_EDITOR_APPLYING, LVUI_EDITOR_APPLIED, LVUI_EDITOR_CANCELED };
enum { LVUI_EDITOR_BEGIN, LVUI_EDITOR_EDIT, LVUI_EDITOR_PREVIEW_READY, LVUI_EDITOR_PREVIEW_FAILED,
       LVUI_EDITOR_APPLY, LVUI_EDITOR_APPLY_SUCCEEDED, LVUI_EDITOR_APPLY_FAILED, LVUI_EDITOR_CANCEL };
enum { LVUI_EDITOR_EFFECT_PREVIEW, LVUI_EDITOR_EFFECT_COMMIT, LVUI_EDITOR_EFFECT_DISCARD_PREVIEW };
enum { LVUI_EDITOR_OK, LVUI_EDITOR_INVALID_INPUT, LVUI_EDITOR_DUPLICATE_ID, LVUI_EDITOR_INVALID_VALUE,
       LVUI_EDITOR_UNAVAILABLE, LVUI_EDITOR_STALE, LVUI_EDITOR_INVALID_PHASE, LVUI_EDITOR_OVERFLOW };

/* Inputs and arrays are caller-owned and must remain immutable during a call.
   Strings are UTF-8 and NUL-terminated; NULL is interpreted as an empty string,
   which is rejected for required identifiers and labels. Flags must be 0 or 1.
   An array may be NULL only when its count is zero; counts cannot exceed 65536.
   Independent requests share no mutable state and may run concurrently. */
typedef struct lvui_editor_menu_action {
    const char *id, *label, *shortcut, *icon_name, *accessibility_name;
    int enabled, visible, checked, kind;
} lvui_editor_menu_action;
typedef struct lvui_editor_choice { const char *id, *label; } lvui_editor_choice;
typedef struct lvui_editor_value {
    /* One LVUI_EDITOR_VALUE_* discriminator. */
    int kind;
    const char *text;
    int64_t integer;
    double real;
    int boolean;
} lvui_editor_value;
typedef struct lvui_editor_field {
    const char *id, *label;
    int kind;
    lvui_editor_value value;
    int enabled, visible, required, has_minimum, has_maximum;
    double minimum, maximum;
    const lvui_editor_choice *choices;
    size_t choice_count;
    const char *validation_error;
    int has_integer_minimum, has_integer_maximum;
    int64_t integer_minimum, integer_maximum;
} lvui_editor_field;
typedef struct lvui_editor_token {
    const char *session;
    uint64_t source_revision, edit_revision;
} lvui_editor_token;
typedef struct lvui_editor_state {
    const char *session;
    uint64_t source_revision, edit_revision;
    int phase, preview_enabled, preview_pending;
    const char *preview_id, *error;
    const lvui_editor_field *original_fields;
    size_t original_count;
    const lvui_editor_field *fields;
    size_t field_count;
} lvui_editor_state;
typedef struct lvui_editor_event {
    int action;
    const char *field_id;
    int has_value;
    lvui_editor_value value;
    lvui_editor_token token;
    const char *preview_id, *error;
} lvui_editor_event;
typedef struct lvui_editor_result lvui_editor_result;

/* Return a new state and explicit work effects as JSON. `kind` selects a panel;
   the function never invokes app work or changes caller-owned state. Invalid
   inputs produce an error result, while allocation failure returns NULL. */
LVUI_COMPILER_EXPORT lvui_editor_result *lvui_editor_panel_execute(
    int kind, const lvui_editor_state *, const lvui_editor_event *) LVUI_EDITOR_NOEXCEPT;
/* Project the supplied command state without executing or subscribing to it. */
LVUI_COMPILER_EXPORT lvui_editor_result *lvui_editor_menu_action_execute(
    const lvui_editor_menu_action *) LVUI_EDITOR_NOEXCEPT;
/* Project an ordered submenu; duplicate item IDs reject the entire result. */
LVUI_COMPILER_EXPORT lvui_editor_result *lvui_editor_submenu_execute(
    const char *parent_id, const lvui_editor_menu_action *, size_t count) LVUI_EDITOR_NOEXCEPT;
/* Project a schema into automatic-control presentation data. The returned
   JSON is a snapshot; callback and viewmodel ownership remain with the host. */
LVUI_COMPILER_EXPORT lvui_editor_result *lvui_editor_auto_form_execute(
    const lvui_editor_field *, size_t count) LVUI_EDITOR_NOEXCEPT;
/* Return LVUI_EDITOR_OK for success or an LVUI_EDITOR_* error code. NULL is an
   allocation failure and has status LVUI_EDITOR_INVALID_INPUT. */
LVUI_COMPILER_EXPORT int lvui_editor_result_status(const lvui_editor_result *) LVUI_EDITOR_NOEXCEPT;
/* Borrow UTF-8 JSON on success or diagnostic text on failure. The pointer is
   valid until free, may contain embedded NULs, and is NULL for a NULL result. */
LVUI_COMPILER_EXPORT const char *lvui_editor_result_data(const lvui_editor_result *) LVUI_EDITOR_NOEXCEPT;
/* Return the byte count of the borrowed data, excluding any terminator. */
LVUI_COMPILER_EXPORT size_t lvui_editor_result_size(const lvui_editor_result *) LVUI_EDITOR_NOEXCEPT;
/* Release the result and its borrowed data; passing NULL is harmless. */
LVUI_COMPILER_EXPORT void lvui_editor_result_free(lvui_editor_result *) LVUI_EDITOR_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef LVUI_EDITOR_NOEXCEPT
