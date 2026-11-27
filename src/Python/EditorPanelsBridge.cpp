#include "EditorPanelsBridge.h"
#include <LVUI/EditorPanels.h>

#include <iomanip>
#include <limits>
#include <locale>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

struct lvui_editor_result final { int status = 0; std::string data; };

namespace {
// Bounds each caller-supplied collection without retaining process-wide state.
constexpr std::size_t maximum_items() noexcept { return 65536; }
std::string value(const char* text) { return text ? std::string(text) : std::string{}; }
bool flag(int input) {
    if (input != 0 && input != 1) throw std::invalid_argument("Expected a boolean flag");
    return input != 0;
}
template<class Work> lvui_editor_result* guard(Work&& work) noexcept {
    auto* result = new (std::nothrow) lvui_editor_result;
    if (!result) return nullptr;
    try { work(*result); }
    catch (const std::exception& ex) { result->status = 1; try { result->data = ex.what(); } catch (...) {} }
    catch (...) { result->status = 1; try { result->data = "Editor bridge failed"; } catch (...) {} }
    return result;
}
template<class T> bool accept(lvui_editor_result& target, const lvui::EditorResult<T>& result) {
    if (result) return true;
    target.status = static_cast<int>(result.error().code) + 1;
    target.data = result.error().message;
    return false;
}

lvui::MenuActionState action(const lvui_editor_menu_action& a) {
    if (a.kind < 0 || a.kind > 2) throw std::invalid_argument("Invalid menu item kind");
    return {value(a.id), value(a.label), value(a.shortcut), value(a.icon_name), value(a.accessibility_name),
        flag(a.enabled), flag(a.visible), flag(a.checked), static_cast<lvui::MenuEntryKind>(a.kind)};
}
lvui::EditorValue editor_value(const lvui_editor_value& input) {
    switch (input.kind) {
    case 0: return value(input.text);
    case 1: return input.integer;
    case 2: return input.real;
    case 3: return flag(input.boolean);
    default: throw std::invalid_argument("Unknown editor value kind");
    }
}
std::vector<lvui::EditorField> fields(const lvui_editor_field* input, std::size_t count) {
    if (count > maximum_items() || (count && !input)) throw std::invalid_argument("Invalid editor field array");
    std::vector<lvui::EditorField> output;
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& f = input[i];
        if (f.kind < 0 || f.kind > 4 || f.choice_count > maximum_items() || (f.choice_count && !f.choices))
            throw std::invalid_argument("Invalid editor field or choice array");
        lvui::EditorField field{value(f.id), value(f.label), static_cast<lvui::EditorFieldKind>(f.kind), editor_value(f.value),
            flag(f.enabled), flag(f.visible), flag(f.required)};
        if (flag(f.has_minimum)) field.minimum = f.minimum;
        if (flag(f.has_maximum)) field.maximum = f.maximum;
        field.choices.reserve(f.choice_count);
        for (std::size_t j = 0; j < f.choice_count; ++j)
            field.choices.push_back({value(f.choices[j].id), value(f.choices[j].label)});
        field.validation_error = value(f.validation_error);
        if (flag(f.has_integer_minimum)) field.integer_minimum = f.integer_minimum;
        if (flag(f.has_integer_maximum)) field.integer_maximum = f.integer_maximum;
        output.push_back(std::move(field));
    }
    return output;
}
lvui::EditorToken token(const lvui_editor_token& t) { return {value(t.session), t.source_revision, t.edit_revision}; }

void quoted(std::string& output, std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    output += '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { output += '\\'; output += static_cast<char>(c); }
        else if (c < 0x20) { output += "\\u00"; output += hex[c >> 4]; output += hex[c & 15]; }
        else output += static_cast<char>(c);
    }
    output += '"';
}
void boolean(std::string& output, bool b) { output += b ? "true" : "false"; }
void integer(std::string& output, std::uint64_t n) { output += std::to_string(n); }
void real(std::string& output, double number) {
    // Floating to_chars requires macOS 13.3; the package also targets 12.
    // A local classic locale avoids the process locale changing JSON syntax.
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << number;
    if (!stream) throw std::runtime_error("Could not encode editor number");
    const auto rendered = stream.str();
    output.append(rendered);
    // JSON must preserve a real value's type when it is numerically integral;
    // otherwise Python would feed an integer back into a real-valued field.
    if (rendered.find_first_of(".eE") == rendered.npos) output += ".0";
}
void json_value(std::string& output, const lvui::EditorValue& value) {
    std::visit([&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::string>) quoted(output, item);
        else if constexpr (std::is_same_v<T, bool>) boolean(output, item);
        else if constexpr (std::is_same_v<T, double>) real(output, item);
        else output += std::to_string(item);
    }, value);
}
void json_fields(std::string& output, const std::vector<lvui::EditorField>& fields) {
    output += '[';
    bool first = true;
    for (const auto& f : fields) {
        if (!std::exchange(first, false)) output += ',';
        output += "{\"id\":"; quoted(output, f.id); output += ",\"label\":"; quoted(output, f.label);
        output += ",\"kind\":"; integer(output, static_cast<unsigned>(f.kind)); output += ",\"value\":"; json_value(output, f.value);
        output += ",\"enabled\":"; boolean(output, f.enabled); output += ",\"visible\":"; boolean(output, f.visible);
        output += ",\"required\":"; boolean(output, f.required); output += ",\"minimum\":";
        if (f.minimum) real(output, *f.minimum); else output += "null";
        output += ",\"maximum\":"; if (f.maximum) real(output, *f.maximum); else output += "null";
        output += ",\"choices\":["; bool first_choice = true;
        for (const auto& c : f.choices) {
            if (!std::exchange(first_choice, false)) output += ',';
            output += "{\"id\":"; quoted(output, c.id); output += ",\"label\":"; quoted(output, c.label); output += '}';
        }
        output += "],\"validation_error\":"; quoted(output, f.validation_error);
        output += ",\"integer_minimum\":";
        if (f.integer_minimum) output += std::to_string(*f.integer_minimum); else output += "null";
        output += ",\"integer_maximum\":";
        if (f.integer_maximum) output += std::to_string(*f.integer_maximum); else output += "null";
        output += '}';
    }
    output += ']';
}
void json_token(std::string& output, const lvui::EditorToken& token) {
    output += "{\"session\":"; quoted(output, token.session); output += ",\"source_revision\":"; integer(output, token.source_revision);
    output += ",\"edit_revision\":"; integer(output, token.edit_revision); output += '}';
}
void json_panel(std::string& output, const lvui::EditorPanelResult& result) {
    const auto& s = result.state;
    output += "{\"kind\":"; integer(output, static_cast<unsigned>(result.kind)); output += ",\"state\":{\"session\":"; quoted(output, s.session);
    output += ",\"source_revision\":"; integer(output, s.source_revision); output += ",\"edit_revision\":"; integer(output, s.edit_revision);
    output += ",\"phase\":"; integer(output, static_cast<unsigned>(s.phase)); output += ",\"preview_enabled\":"; boolean(output, s.preview_enabled);
    output += ",\"preview_pending\":"; boolean(output, s.preview_pending); output += ",\"preview_id\":"; quoted(output, s.preview_id);
    output += ",\"error\":"; quoted(output, s.error); output += ",\"original_fields\":"; json_fields(output, s.original_fields);
    output += ",\"fields\":"; json_fields(output, s.fields); output += "},\"effects\":[";
    bool first = true;
    for (const auto& e : result.effects) {
        if (!std::exchange(first, false)) output += ',';
        output += "{\"kind\":"; integer(output, static_cast<unsigned>(e.kind)); output += ",\"token\":"; json_token(output, e.token);
        output += ",\"fields\":"; json_fields(output, e.fields); output += ",\"preview_id\":"; quoted(output, e.preview_id); output += '}';
    }
    output += "]}";
}
void json_menu(std::string& output, const lvui::MenuEntry& item) {
    output += "{\"id\":"; quoted(output, item.key); output += ",\"label\":"; quoted(output, item.label);
    output += ",\"shortcut\":"; quoted(output, item.shortcut); output += ",\"icon_name\":"; quoted(output, item.icon_name);
    output += ",\"accessibility_name\":"; quoted(output, item.accessibility_name); output += ",\"enabled\":"; boolean(output, item.enabled);
    output += ",\"visible\":"; boolean(output, item.visible); output += ",\"checked\":"; boolean(output, item.state == lvui::MenuEntryState::selected);
    output += ",\"kind\":"; integer(output, static_cast<unsigned>(item.kind)); output += '}';
}
}

extern "C" {
lvui_editor_result* lvui_editor_panel_execute(int kind, const lvui_editor_state* input, const lvui_editor_event* event) noexcept {
    return guard([&](auto& result) {
        if (!input || !event || kind < 0 || kind > 8 || input->phase < 0 || input->phase > 4 || event->action < 0 || event->action > 7)
            throw std::invalid_argument("Invalid editor panel request");
        lvui::EditorPanelState state;
        state.session = value(input->session); state.source_revision = input->source_revision; state.edit_revision = input->edit_revision;
        state.phase = static_cast<lvui::EditorPanelPhase>(input->phase);
        state.preview_enabled = flag(input->preview_enabled); state.preview_pending = flag(input->preview_pending);
        state.preview_id = value(input->preview_id); state.error = value(input->error);
        state.original_fields = fields(input->original_fields, input->original_count);
        state.fields = fields(input->fields, input->field_count);
        lvui::EditorPanelEvent request_event;
        request_event.action = static_cast<lvui::EditorPanelAction>(event->action);
        request_event.field_id = value(event->field_id);
        if (flag(event->has_value)) request_event.value = editor_value(event->value);
        request_event.token = token(event->token); request_event.preview_id = value(event->preview_id); request_event.error = value(event->error);
        auto response = lvui::detail::execute_editor_panel(static_cast<lvui::EditorPanelKind>(kind), {std::move(state), std::move(request_event)});
        if (accept(result, response)) json_panel(result.data, *response);
    });
}
lvui_editor_result* lvui_editor_menu_action_execute(const lvui_editor_menu_action* input) noexcept {
    return guard([&](auto& result) {
        if (!input) throw std::invalid_argument("Missing menu action");
        auto response = lvui::MenuActionStateBinding{}.execute(action(*input));
        if (accept(result, response)) json_menu(result.data, *response);
    });
}
lvui_editor_result* lvui_editor_submenu_execute(const char* parent_id, const lvui_editor_menu_action* input, size_t count) noexcept {
    return guard([&](auto& result) {
        if (count > maximum_items() || (count && !input)) throw std::invalid_argument("Invalid submenu array");
        lvui::MenuSubmenuItemsRequest request; request.parent_id = value(parent_id); request.items.reserve(count);
        for (std::size_t i = 0; i < count; ++i) request.items.push_back(action(input[i]));
        auto response = lvui::MenuSubmenuItemsBinding{}.execute(request);
        if (!accept(result, response)) return;
        result.data += '['; bool first = true;
        for (const auto& item : *response) { if (!std::exchange(first, false)) result.data += ','; json_menu(result.data, item); }
        result.data += ']';
    });
}
lvui_editor_result* lvui_editor_auto_form_execute(const lvui_editor_field* input, size_t count) noexcept {
    return guard([&](auto& result) {
        auto response = lvui::EditorAutoFormBinding{}.execute({fields(input, count), {}});
        if (!accept(result, response)) return;
        auto& out = result.data;
        out += '['; bool first = true;
        for (const auto& row : *response) {
            if (!std::exchange(first, false)) out += ',';
            out += "{\"id\":"; quoted(out, row->id); out += ",\"label\":"; quoted(out, row->label);
            out += ",\"text\":"; quoted(out, row->text); out += ",\"error\":"; quoted(out, row->error);
            out += ",\"visible\":"; boolean(out, row->visible); out += ",\"enabled\":"; boolean(out, row->enabled);
            out += ",\"boolean\":"; boolean(out, row->boolean); out += ",\"choice\":"; boolean(out, row->choice);
            out += ",\"number\":"; boolean(out, row->number); out += ",\"bounded\":"; boolean(out, row->bounded);
            out += ",\"checked\":"; boolean(out, row->checked); out += ",\"has_error\":"; boolean(out, row->hasError);
            out += ",\"minimum\":"; real(out, row->minimum); out += ",\"maximum\":"; real(out, row->maximum);
            out += ",\"amount\":"; real(out, row->amount); out += ",\"selected\":"; out += std::to_string(row->selected);
            out += ",\"options\":["; bool first_option = true;
            for (const auto& option : row->options) {
                if (!std::exchange(first_option, false)) out += ',';
                quoted(out, option);
            }
            out += "]}";
        }
        out += ']';
    });
}
int lvui_editor_result_status(const lvui_editor_result* result) noexcept { return result ? result->status : 1; }
const char* lvui_editor_result_data(const lvui_editor_result* result) noexcept { return result ? result->data.data() : nullptr; }
size_t lvui_editor_result_size(const lvui_editor_result* result) noexcept { return result ? result->data.size() : 0; }
void lvui_editor_result_free(lvui_editor_result* result) noexcept { delete result; }
}
