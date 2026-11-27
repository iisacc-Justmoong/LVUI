#pragma once

#include <LVUI/Menu.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lvui {

/// An invalid request is rejected without modifying caller-owned data.
/// Error messages are diagnostics; applications supply localized UI messages.
enum class EditorErrorCode { invalid_input, duplicate_id, invalid_value, unavailable, stale, invalid_phase, overflow };
struct EditorError final {
    EditorErrorCode code;
    std::string message;
};
template<class T> using EditorResult = std::expected<T, EditorError>;

/// App-owned menu state. IDs identify commands; labels and shortcuts are display
/// data and never imply a command, resource, or application-wide authority.
struct MenuActionState final {
    std::string id, label, shortcut, icon_name, accessibility_name;
    bool enabled = false, visible = true, checked = false;
    MenuEntryKind kind = MenuEntryKind::action;
};
/// Projects one immutable app state into an existing LVUI menu item. The app
/// observes state changes and invokes this object again; no subscription, menu
/// execution, global lookup or external mutation takes place here.
class MenuActionStateBinding final {
public:
    /// Return a display snapshot or a validation error; the input stays owned
    /// by the caller, and independent calls require no synchronization.
    [[nodiscard]] EditorResult<MenuEntry> execute(const MenuActionState&) const;
};

/// One submenu's complete app-provided, ordered item snapshot. A change event
/// and the lifetime of this snapshot are controlled by the application.
struct MenuSubmenuItemsRequest final {
    std::string parent_id;
    std::vector<MenuActionState> items;
};
/// Projects a submenu atomically: duplicate or malformed identities fail and
/// never return a partial replacement. It does not invoke menu actions.
class MenuSubmenuItemsBinding final {
public:
    /// Return all projected items in caller order, or reject the request
    /// without publishing a partial list. No external state is accessed.
    [[nodiscard]] EditorResult<std::vector<MenuEntry>> execute(const MenuSubmenuItemsRequest&) const;
};

/// Supported generic field presentations. Apps own the field names, choices,
/// labels, constraints, localization and all domain-specific interpretation.
enum class EditorFieldKind { text, integer, real, boolean, choice };
using EditorValue = std::variant<std::string, std::int64_t, double, bool>;
struct EditorChoice final {
    std::string id, label;
    bool operator==(const EditorChoice&) const = default;
};
/// One app-supplied field and draft value. Numeric limits are inclusive. Choice
/// values are stable IDs rather than labels. Required text/choice values may be
/// empty while editing, but cannot be previewed or committed. The caller owns
/// this value.
struct EditorField final {
    std::string id, label;
    EditorFieldKind kind = EditorFieldKind::text;
    EditorValue value = std::string{};
    bool enabled = true, visible = true, required = false;
    std::optional<double> minimum, maximum;
    std::vector<EditorChoice> choices;
    /// Application-provided validation failure; a nonempty value blocks apply.
    std::string validation_error;
    /// Optional exact 64-bit integer limits. A side cannot also specify its
    /// floating-point limit; other field types cannot use these limits.
    std::optional<std::int64_t> integer_minimum, integer_maximum;
    bool operator==(const EditorField&) const = default;
};

/// One input from an automatically generated field. An incomplete or invalid
/// number has no typed value; its original text is still delivered to the app.
/// The app controls validation feedback, ordering, lifetime and document edits.
struct EditorFieldEdit final {
    std::string field_id;
    EditorFieldKind kind;
    std::optional<EditorValue> value;
    std::string raw_text;
};

/// Caller-owned presentation data produced from one validated schema field.
/// Publish replacement rows instead of mutating a row already observed by a
/// view. Injected event functions run only on user input; the host owns the
/// destination, lifetime and serialization of its own state.
struct EditorAutoField final {
    std::string id, label, text, error;
    bool visible = true, enabled = true, boolean = false, choice = false;
    bool number = false, bounded = false, checked = false, hasError = false;
    double minimum = 0, maximum = 0, amount = 0;
    int selected = 0;
    std::vector<std::string> options;
    std::function<void(std::string)> textEdited;
    std::function<void(bool)> booleanEdited;
    std::function<void(double)> numberEdited;
    std::function<void(int)> choiceEdited;
};

/// App-owned viewmodel whose observable collection is populated with the
/// projection result. Updating it republishes the keyed field list; the app
/// controls the model's lifetime, observation and synchronization.
struct EditorAutoFormModel final {
    ii::core::Observable<std::vector<std::shared_ptr<EditorAutoField>>> fields;
};

/// The event sink is an explicit dependency and is copied into the returned
/// callbacks. The application must keep its captured dependencies valid for
/// the lifetime of the view, or use a weak lifetime guard.
struct EditorAutoFormRequest final {
    std::vector<EditorField> fields;
    std::function<void(EditorFieldEdit)> on_edit;
};

/// Maps an app schema to the data and event adapters consumed by the LVUI
/// automatic field View. No app callback runs during projection, and invalid
/// input returns an error without publishing a partial field collection.
class EditorAutoFormBinding final {
public:
    /// Return owned presentation rows, retaining only the explicit event sink.
    /// Independent calls share no mutable state. UI events must be serialized
    /// by the owner and can outlive this short-lived projection object.
    [[nodiscard]] EditorResult<std::vector<std::shared_ptr<EditorAutoField>>>
    execute(const EditorAutoFormRequest&) const;
};

enum class EditorPanelKind { menu_parameters, preview, mask_refinement, layer_content,
                             filter_gallery, color_profile, soft_proof, timeline, export_format };
enum class EditorPanelPhase { fresh, editing, applying, applied, canceled };
enum class EditorPanelAction { begin, edit, preview_ready, preview_failed, apply, apply_succeeded, apply_failed, cancel };
enum class EditorEffectKind { preview, commit, discard_preview };

/// An app activation, source revision and edit revision jointly identify an
/// asynchronous result. The host must give every new activation a fresh ID.
struct EditorToken final {
    std::string session;
    std::uint64_t source_revision = 0, edit_revision = 0;
    bool operator==(const EditorToken&) const = default;
};
/// Caller-owned snapshot. Original fields are captured by `begin`; drafts
/// never alter the source document. The caller serializes concurrent access.
struct EditorPanelState final {
    std::string session;
    std::uint64_t source_revision = 0, edit_revision = 0;
    EditorPanelPhase phase = EditorPanelPhase::fresh;
    bool preview_enabled = false, preview_pending = false;
    std::string preview_id, error;
    std::vector<EditorField> original_fields, fields;
};
/// Explicit input from the host. Preview and commit completions must carry the
/// token returned with their effect; a late activation or revision is rejected.
struct EditorPanelEvent final {
    EditorPanelAction action = EditorPanelAction::begin;
    std::string field_id;
    std::optional<EditorValue> value;
    EditorToken token;
    std::string preview_id, error;
};
struct EditorPanelRequest final {
    EditorPanelState state;
    EditorPanelEvent event;
};
/// Data-only work instruction. Apps perform preview, commit and disposal in
/// their own execution domain; effects never access an owner or execute a task.
struct EditorEffect final {
    EditorEffectKind kind;
    EditorToken token;
    std::vector<EditorField> fields;
    std::string preview_id;
};
struct EditorPanelResult final {
    EditorPanelKind kind;
    EditorPanelState state;
    std::vector<EditorEffect> effects;
};

namespace detail {
/// Implements the shared data-only transition; the panel never calls another
/// execution object. Invalid events return an error and preserve input values.
[[nodiscard]] EditorResult<EditorPanelResult> execute_editor_panel(EditorPanelKind, const EditorPanelRequest&);
}
/// Each named panel has one execution entry. It returns a new state and explicit
/// effects, is safe for independent concurrent calls, and keeps no references.
template<EditorPanelKind Kind> class EditorPanelBinding final {
public:
    /// Return the next owned snapshot and work effects. The caller orders
    /// concurrent events and performs each effect; validation failures leave
    /// the supplied request unchanged and expose an explicit error.
    [[nodiscard]] EditorResult<EditorPanelResult> execute(const EditorPanelRequest& request) const {
        return detail::execute_editor_panel(Kind, request);
    }
};
/// Drafts the caller's command parameters; execution remains app-owned.
using MenuCommandParameterForm = EditorPanelBinding<EditorPanelKind::menu_parameters>;
/// Requests an isolated preview for every valid edit and never commits itself.
using NonDestructivePreviewPanel = EditorPanelBinding<EditorPanelKind::preview>;
/// Drafts the app's mask-refinement fields without applying a mask.
using MaskRefinementControls = EditorPanelBinding<EditorPanelKind::mask_refinement>;
/// Drafts layer-content options without owning a layer or document.
using LayerContentOptionsPanel = EditorPanelBinding<EditorPanelKind::layer_content>;
/// Drafts app-provided filter selections; rendering and thumbnails are external.
using FilterGalleryPanel = EditorPanelBinding<EditorPanelKind::filter_gallery>;
/// Drafts an app-provided color-profile selection without discovering profiles.
using ColorProfilePicker = EditorPanelBinding<EditorPanelKind::color_profile>;
/// Drafts proof settings without performing a color transform.
using SoftProofSettingsPanel = EditorPanelBinding<EditorPanelKind::soft_proof>;
/// Drafts app-provided timeline edit options without changing a timeline.
using TimelineEditOptionsPanel = EditorPanelBinding<EditorPanelKind::timeline>;
/// Drafts app-provided format options without writing or exporting a file.
using ExportFormatOptionsPanel = EditorPanelBinding<EditorPanelKind::export_format>;

} // namespace lvui
