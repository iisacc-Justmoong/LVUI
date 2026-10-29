#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/NativeColorPickerValues.h>
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
std::string text(ii::gui::Widget field) {
    const auto snapshot = take(field.snapshot()); return take(field.text_content({0, snapshot.text_length})).text;
}
}

int main() {
    using namespace ii::gui;
    const lvui::StyleColor red{1, 0, 0, .5}, blue{0, 0, 1, 1};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 100}));
    auto editor = std::make_shared<lvui::ColorSelection>(red);
    auto picker = std::make_shared<lvui::ColorPicker>(editor);
    auto source = std::make_shared<lvui::ColorSelectionSurface>(editor);
    auto session = std::make_shared<lvui::ColorPickerSession>(picker, source, source);
    lvui::native::ColorPickerValuesInput input; input.session = session;
    std::optional<lvui::native::NativeColorPickerValues> values; Widget other; std::vector<lvui::StyleColor> edits;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
        auto row = lvui::native::NativeColorPickerValues::create(builder, *root, "values", input,
            [&](lvui::StyleColor color) { edits.push_back(color); });
        if (!row) return std::unexpected(row.error()); values = *row;
        auto button = builder.action_surface(*root, "other", {.text = "Other", .width = 60, .height = 22});
        if (!button) return std::unexpected(button.error()); other = *button; return {};
    }));
    auto hex = values->hex_field(), alpha = values->alpha_field();
    require(text(hex) == "FF0000" && text(alpha) == "50", "Picker value fields did not show the initial color");
    const auto replace = [&](Widget field, std::string value) {
        good(field.focus()); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::select_all}));
        good(tree->input({.kind = WidgetInputKind::text, .text = std::move(value)}));
    };
    replace(hex, "00FF00"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(editor->snapshot().color == lvui::StyleColor{0, 1, 0, .5} && picker->active() && edits.size() == 1,
            "HEX input did not start a session and preserve alpha");
    replace(hex, "FF0000"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(editor->snapshot().color == red && edits.size() == 2, "Returning to the baseline did not publish the edit");
    replace(alpha, "25"); good(other.focus());
    require(near(editor->snapshot().color.alpha, .25) && text(alpha) == "25" && edits.size() == 3,
            "Alpha did not commit on focus loss");
    require(bool(picker->cancel()), "Picker values session could not cancel"); good(values->set_options(input));
    require(editor->snapshot().color == red && text(hex) == "FF0000" && text(alpha) == "50",
            "Canceled picker values were not restored");
    replace(hex, "invalid"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(text(hex) == "FF0000" && !picker->active() && edits.size() == 3, "Invalid HEX began an edit or remained displayed");
    replace(hex, "123456"); require(bool(editor->set_color(blue)), "External color replacement failed");
    input.source_revision = std::int64_t(editor->revision()); good(values->set_options(input));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(editor->snapshot().color == blue && text(hex) == "0000FF" && edits.size() == 3,
            "Stale HEX input overwrote an external replacement");
    replace(hex, "11223380"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(near(editor->snapshot().color.alpha, 128. / 255) && text(alpha) == "50" && edits.size() == 4,
            "Eight-digit HEX did not update alpha");
    const auto drawing = take(tree->drawing_snapshot({400, 100}));
    bool comparison = false;
    const auto check = [&](const auto& panels) { for (const auto& panel : panels) {
        if (near(panel.bounds.width, 20) && near(panel.bounds.height, 28) && near(panel.top.alpha, 128. / 255)) comparison = true;
    }};
    check(drawing.panels); for (const auto& layer : drawing.overlays) check(layer.panels);
    require(comparison, "Picker comparison did not submit the current alpha in its swatch drawing");
    bool rejected = false;
    std::thread worker([&] { auto result = values->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Picker values accepted a foreign-thread update");
    good(host.clear_content()); require(!values->set_options(input), "Retired picker values remained writable");
    tree->close();
}
