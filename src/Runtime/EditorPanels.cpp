#include "EditorPanels.h"

#include <iiSDKCore/Core/Text/Utf8.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace lvui {
namespace {
EditorError error(EditorErrorCode code, std::string message) { return {code, std::move(message)}; }
bool text(std::string_view value, bool empty = false) {
    return (empty || !value.empty()) && value.find('\0') == value.npos && ii::core::valid_utf8(value);
}

EditorResult<MenuEntry> project(const MenuActionState& source) {
    if ((source.kind != MenuEntryKind::action && source.kind != MenuEntryKind::submenu && source.kind != MenuEntryKind::divider) ||
        !text(source.id) || !text(source.label, source.kind == MenuEntryKind::divider) || !text(source.shortcut, true) ||
        !text(source.icon_name, true) || !text(source.accessibility_name, true))
        return std::unexpected(error(EditorErrorCode::invalid_input, "Invalid menu action text"));
    MenuEntry entry;
    entry.key = source.id;
    entry.label = source.label;
    entry.shortcut = source.shortcut;
    entry.icon_name = source.icon_name;
    entry.accessibility_name = source.accessibility_name;
    entry.kind = source.kind;
    entry.enabled = source.enabled;
    entry.visible = source.visible;
    entry.state = source.checked ? MenuEntryState::selected : MenuEntryState::normal;
    return entry;
}

bool valid_field(const EditorField& field, bool complete) {
    if (!text(field.id) || !text(field.label) || !text(field.validation_error, true)) return false;
    if ((field.minimum && !std::isfinite(*field.minimum)) || (field.maximum && !std::isfinite(*field.maximum)) ||
        (field.minimum && field.maximum && *field.minimum > *field.maximum)) return false;
    if ((field.integer_minimum && field.minimum) || (field.integer_maximum && field.maximum) ||
        (field.integer_minimum && field.integer_maximum && *field.integer_minimum > *field.integer_maximum) ||
        (field.kind != EditorFieldKind::integer && (field.integer_minimum || field.integer_maximum))) return false;
    const auto number_valid = [&](long double value) {
        return (!field.minimum || value >= static_cast<long double>(*field.minimum)) &&
               (!field.maximum || value <= static_cast<long double>(*field.maximum));
    };
    if (field.kind != EditorFieldKind::choice && !field.choices.empty()) return false;
    if (field.kind != EditorFieldKind::integer && field.kind != EditorFieldKind::real && (field.minimum || field.maximum)) return false;
    switch (field.kind) {
    case EditorFieldKind::text: {
        const auto* value = std::get_if<std::string>(&field.value);
        return value && text(*value, true) && (!complete || !field.required || !value->empty());
    }
    case EditorFieldKind::integer: {
        const auto* value = std::get_if<std::int64_t>(&field.value);
        if (!value) return false;
        if ((field.integer_minimum && *value < *field.integer_minimum) ||
            (field.integer_maximum && *value > *field.integer_maximum)) return false;
        // Bounds are IEEE doubles, but the integer may exceed their exact
        // precision. Round the bound instead of rounding the integer value.
        constexpr double upper_exclusive = 0x1p63;
        if (field.minimum) {
            const auto lower = std::ceil(*field.minimum);
            if (lower >= upper_exclusive || (lower > -upper_exclusive && *value < static_cast<std::int64_t>(lower))) return false;
        }
        if (field.maximum) {
            const auto upper = std::floor(*field.maximum);
            if (upper < -upper_exclusive || (upper < upper_exclusive && *value > static_cast<std::int64_t>(upper))) return false;
        }
        return true;
    }
    case EditorFieldKind::real: {
        const auto* value = std::get_if<double>(&field.value);
        return value && std::isfinite(*value) && number_valid(*value);
    }
    case EditorFieldKind::boolean:
        return std::holds_alternative<bool>(field.value);
    case EditorFieldKind::choice: {
        const auto* value = std::get_if<std::string>(&field.value);
        if (!value || !text(*value, true)) return false;
        std::unordered_set<std::string> ids;
        for (const auto& option : field.choices)
            if (!text(option.id) || !text(option.label) || !ids.insert(option.id).second) return false;
        if (value->empty()) return !complete || !field.required;
        return ids.contains(*value);
    }
    }
    return false;
}

bool valid_fields(const std::vector<EditorField>& fields, bool complete) {
    std::unordered_set<std::string> ids;
    for (const auto& field : fields)
        if (!valid_field(field, complete) || !ids.insert(field.id).second || (complete && !field.validation_error.empty())) return false;
    return true;
}

bool same_schema(const EditorField& left, const EditorField& right) {
    return left.id == right.id && left.label == right.label && left.kind == right.kind &&
        left.enabled == right.enabled && left.visible == right.visible && left.required == right.required &&
        left.minimum == right.minimum && left.maximum == right.maximum && left.choices == right.choices &&
        left.integer_minimum == right.integer_minimum && left.integer_maximum == right.integer_maximum;
}

EditorToken token(const EditorPanelState& state) { return {state.session, state.source_revision, state.edit_revision}; }
EditorEffect effect(EditorEffectKind kind, const EditorPanelState& state) {
    return {kind, token(state), state.fields, state.preview_id};
}
}

EditorResult<MenuEntry> MenuActionStateBinding::execute(const MenuActionState& source) const {
    return project(source);
}

EditorResult<std::vector<MenuEntry>> MenuSubmenuItemsBinding::execute(const MenuSubmenuItemsRequest& request) const {
    if (!text(request.parent_id)) return std::unexpected(error(EditorErrorCode::invalid_input, "Missing submenu identity"));
    std::vector<MenuEntry> result;
    std::unordered_set<std::string> ids;
    result.reserve(request.items.size());
    for (const auto& item : request.items) {
        auto entry = project(item);
        if (!entry) return std::unexpected(entry.error());
        if (!ids.insert(entry->key).second) return std::unexpected(error(EditorErrorCode::duplicate_id, "Duplicate submenu item"));
        result.push_back(std::move(*entry));
    }
    return result;
}

EditorResult<std::vector<std::shared_ptr<EditorAutoField>>>
EditorAutoFormBinding::execute(const EditorAutoFormRequest& request) const {
    if (!valid_fields(request.fields, false))
        return std::unexpected(error(EditorErrorCode::invalid_input, "Invalid editor field schema"));
    std::vector<std::shared_ptr<EditorAutoField>> rows;
    rows.reserve(request.fields.size());
    for (const auto& field : request.fields) {
        // These are the existing keyed-row, accessible-text and selector
        // contracts of the AOT native controls. Reject before publishing any
        // rows, rather than returning a form that the host cannot mount.
        constexpr std::size_t max_key_bytes = 1024, max_label_bytes = 65536;
        constexpr std::size_t max_choices = 400, max_choice_bytes = 8192, max_choice_total_bytes = 1024 * 1024;
        const bool empty_choice = field.kind == EditorFieldKind::choice &&
            (!field.required || std::get<std::string>(field.value).empty());
        if (field.id.size() > max_key_bytes || field.label.size() > max_label_bytes || field.validation_error.size() > max_label_bytes)
            return std::unexpected(error(EditorErrorCode::overflow, "Editor field identity or label exceeds the native control limit"));
        if (field.choices.size() > max_choices - static_cast<std::size_t>(empty_choice))
            return std::unexpected(error(EditorErrorCode::overflow, "Too many choices for the native selector"));
        std::size_t choice_bytes = 0;
        for (const auto& option : field.choices) {
            if (option.label.size() > max_choice_bytes || option.label.size() > max_choice_total_bytes - choice_bytes)
                return std::unexpected(error(EditorErrorCode::overflow, "Choice labels exceed the native selector limit"));
            choice_bytes += option.label.size();
        }
        auto row = std::make_shared<EditorAutoField>();
        row->id = field.id; row->label = field.label; row->error = field.validation_error;
        row->hasError = !field.validation_error.empty(); row->visible = field.visible; row->enabled = field.enabled;
        row->boolean = field.kind == EditorFieldKind::boolean;
        row->choice = field.kind == EditorFieldKind::choice;
        row->number = field.kind == EditorFieldKind::real || field.kind == EditorFieldKind::integer;
        // Wider valid real ranges use a numeric text field: iiGUI sliders only
        // admit endpoints in [-1e12, 1e12]. Validation still uses the schema.
        row->bounded = field.kind == EditorFieldKind::real && field.minimum && field.maximum &&
            *field.minimum < *field.maximum && *field.minimum >= -1.0e12 && *field.maximum <= 1.0e12;
        if (field.minimum) row->minimum = *field.minimum;
        if (field.maximum) row->maximum = *field.maximum;
        if (const auto* value = std::get_if<std::string>(&field.value)) row->text = *value;
        if (const auto* value = std::get_if<std::int64_t>(&field.value)) row->text = std::to_string(*value);
        if (const auto* value = std::get_if<double>(&field.value)) {
            row->amount = *value;
            std::ostringstream formatted;
            formatted.imbue(std::locale::classic());
            formatted << std::setprecision(std::numeric_limits<double>::max_digits10) << *value;
            if (!formatted) return std::unexpected(error(EditorErrorCode::invalid_value, "Cannot format real value"));
            row->text = formatted.str();
        }
        if (const auto* value = std::get_if<bool>(&field.value)) row->checked = *value;
        std::vector<std::string> ids;
        // The native selector requires a nonnegative index. A blank option
        // represents a missing value and also lets optional fields be cleared.
        if (empty_choice) { ids.emplace_back(); row->options.emplace_back(); }
        for (const auto& option : field.choices) {
            if (std::get<std::string>(field.value) == option.id) row->selected = static_cast<int>(ids.size());
            ids.push_back(option.id); row->options.push_back(option.label);
        }
        const auto sink = request.on_edit;
        const auto id = field.id;
        const auto kind = field.kind;
        const bool accepts_input = field.enabled && field.visible;
        row->textEdited = [sink, id, kind, accepts_input](std::string input) {
            if (!sink || !accepts_input) return;
            EditorFieldEdit event{id, kind, {}, input};
            if (kind == EditorFieldKind::text) event.value = input;
            else if (kind == EditorFieldKind::integer) {
                std::int64_t value{};
                const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
                if (parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size()) event.value = value;
            } else if (kind == EditorFieldKind::real) {
                double value{};
                std::istringstream parsed(input);
                parsed.imbue(std::locale::classic());
                if ((parsed >> std::noskipws >> value) && parsed.peek() == std::char_traits<char>::eof() && std::isfinite(value))
                    event.value = value;
            }
            sink(std::move(event));
        };
        row->booleanEdited = [sink, id, kind, accepts_input](bool value) {
            if (sink && accepts_input && kind == EditorFieldKind::boolean) sink({id, kind, EditorValue{value}, {}});
        };
        row->numberEdited = [sink, id, kind, accepts_input](double value) {
            if (sink && accepts_input && kind == EditorFieldKind::real && std::isfinite(value)) sink({id, kind, EditorValue{value}, {}});
        };
        row->choiceEdited = [sink, id, kind, accepts_input, ids = std::move(ids)](int index) {
            if (sink && accepts_input && kind == EditorFieldKind::choice && index >= 0 && static_cast<std::size_t>(index) < ids.size())
                sink({id, kind, EditorValue{ids[static_cast<std::size_t>(index)]}, {}});
        };
        rows.push_back(std::move(row));
    }
    return rows;
}

EditorResult<EditorPanelResult> detail::execute_editor_panel(EditorPanelKind kind, const EditorPanelRequest& request) {
    const auto& input = request.state;
    const auto& event = request.event;
    if (!text(input.session) || !text(input.preview_id, true) || !text(input.error, true) ||
        !valid_fields(input.fields, false))
        return std::unexpected(error(EditorErrorCode::invalid_input, "Invalid editor session or field schema"));
    EditorPanelResult result{kind, input, {}};
    auto& state = result.state;
    if (kind == EditorPanelKind::preview) state.preview_enabled = true;
    if (event.action == EditorPanelAction::begin) {
        if (state.phase != EditorPanelPhase::fresh || state.edit_revision || state.preview_pending ||
            !state.preview_id.empty() || !state.original_fields.empty())
            return std::unexpected(error(EditorErrorCode::invalid_phase, "Session already initialized"));
        state.original_fields = state.fields;
        state.phase = EditorPanelPhase::editing;
        state.error.clear();
        return result;
    }
    if (state.original_fields.size() != state.fields.size() || !valid_fields(state.original_fields, false) ||
        !std::equal(state.original_fields.begin(), state.original_fields.end(), state.fields.begin(), same_schema))
        return std::unexpected(error(EditorErrorCode::invalid_input, "Draft schema differs from its source snapshot"));
    const auto matching = [&] { return event.token == token(state); };
    switch (event.action) {
    case EditorPanelAction::edit: {
        if (state.phase != EditorPanelPhase::editing) break;
        const auto found = std::find_if(state.fields.begin(), state.fields.end(), [&](const auto& field) { return field.id == event.field_id; });
        if (found == state.fields.end()) return std::unexpected(error(EditorErrorCode::invalid_input, "Unknown field"));
        if (!found->enabled || !found->visible) return std::unexpected(error(EditorErrorCode::unavailable, "Field cannot be edited"));
        if (!event.value) return std::unexpected(error(EditorErrorCode::invalid_value, "Missing field value"));
        auto candidate = *found;
        candidate.value = *event.value;
        candidate.validation_error.clear();
        if (!valid_field(candidate, false)) return std::unexpected(error(EditorErrorCode::invalid_value, "Field value violates its app-provided constraints"));
        if (candidate.value == found->value && found->validation_error.empty()) return result;
        if (state.edit_revision == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(error(EditorErrorCode::overflow, "Editor revision overflow"));
        *found = std::move(candidate);
        ++state.edit_revision;
        state.error.clear();
        if (state.preview_enabled && valid_fields(state.fields, true)) {
            state.preview_pending = true;
            result.effects.push_back(effect(EditorEffectKind::preview, state));
        } else {
            if (state.preview_pending || !state.preview_id.empty()) result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
            state.preview_pending = false;
            state.preview_id.clear();
        }
        return result;
    }
    case EditorPanelAction::preview_ready:
    case EditorPanelAction::preview_failed:
        if (state.phase != EditorPanelPhase::editing) break;
        if (!state.preview_pending || !matching()) return std::unexpected(error(EditorErrorCode::stale, "Stale preview result"));
        if (event.action == EditorPanelAction::preview_ready) {
            if (!text(event.preview_id)) return std::unexpected(error(EditorErrorCode::invalid_input, "Preview identity is required"));
            if (!state.preview_id.empty() && state.preview_id != event.preview_id)
                result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
            state.preview_id = event.preview_id;
            state.error.clear();
        } else {
            if (!text(event.error)) return std::unexpected(error(EditorErrorCode::invalid_input, "Preview failure is required"));
            if (!state.preview_id.empty()) result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
            state.preview_id.clear();
            state.error = event.error;
        }
        state.preview_pending = false;
        return result;
    case EditorPanelAction::apply:
        if (state.phase != EditorPanelPhase::editing) break;
        if (!valid_fields(state.fields, true)) return std::unexpected(error(EditorErrorCode::invalid_value, "Incomplete or invalid editor values"));
        // While a newer preview is outstanding, the visible preview belongs to
        // an older draft. Do not leave that image behind if commit later fails.
        if (state.preview_pending && !state.preview_id.empty()) {
            result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
            state.preview_id.clear();
        }
        state.phase = EditorPanelPhase::applying;
        state.preview_pending = false;
        state.error.clear();
        result.effects.push_back(effect(EditorEffectKind::commit, state));
        return result;
    case EditorPanelAction::apply_succeeded:
    case EditorPanelAction::apply_failed:
        if (state.phase != EditorPanelPhase::applying) break;
        if (!matching()) return std::unexpected(error(EditorErrorCode::stale, "Stale commit result"));
        if (event.action == EditorPanelAction::apply_failed) {
            if (!text(event.error)) return std::unexpected(error(EditorErrorCode::invalid_input, "Commit failure is required"));
            state.phase = EditorPanelPhase::editing;
            state.error = event.error;
            // A preview that was in flight at commit time may already have
            // been rejected while applying; request a fresh one explicitly.
            if (state.preview_enabled && state.preview_id.empty()) {
                state.preview_pending = true;
                result.effects.push_back(effect(EditorEffectKind::preview, state));
            }
        } else {
            result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
            state.preview_id.clear();
            state.phase = EditorPanelPhase::applied;
        }
        return result;
    case EditorPanelAction::cancel:
        if (state.phase != EditorPanelPhase::editing) break;
        result.effects.push_back(effect(EditorEffectKind::discard_preview, state));
        state.fields = state.original_fields;
        state.preview_pending = false;
        state.preview_id.clear();
        state.error.clear();
        state.phase = EditorPanelPhase::canceled;
        return result;
    case EditorPanelAction::begin: break;
    }
    return std::unexpected(error(EditorErrorCode::invalid_phase, "Editor action is unavailable in this phase"));
}
} // namespace lvui
