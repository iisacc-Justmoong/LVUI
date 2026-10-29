#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/NativeColorPicker.h>
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
    const lvui::StyleColor red{1, 0, 0, .5}, blue{0, 0, 1, 1}, green{0, 1, 0, .5};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {600, 700}));
    auto editor = std::make_shared<lvui::ColorSelection>(red);
    auto picker = std::make_shared<lvui::ColorPicker>(editor);
    auto source = std::make_shared<lvui::ColorSelectionSurface>(editor);
    auto session = std::make_shared<lvui::ColorPickerSession>(picker, source, source);
    lvui::native::ColorPickerInput input; input.session = session; input.configuration.recent_colors = {blue};
    std::optional<lvui::native::NativeColorPicker> view;
    std::vector<WidgetColor> edits, accepts; unsigned canceled = 0, eyedropper = 0;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto container = builder.vstack({}, "column", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!container) return std::unexpected(container.error());
        auto value = lvui::native::NativeColorPicker::create(builder, *container, "picker", input,
            {.edited = [&](WidgetColor color) { edits.push_back(color); },
             .accepted = [&](WidgetColor color) { accepts.push_back(color); },
             .canceled = [&] { ++canceled; }, .eyedropper_requested = [&] { ++eyedropper; }});
        if (!value) return std::unexpected(value.error()); view = *value; return {};
    }));
    auto root = take(view->root().snapshot());
    require(near(root.size.width, 320) && near(root.size.height, 461), "Default picker geometry changed");
    require(text(view->hex_field()) == "FF0000", "Default picker color was not shown");
    require(!take(view->rail().interaction()).visible, "Default picker unexpectedly showed a rail");
    const auto replace = [&](Widget field, std::string value) {
        good(field.focus()); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::select_all}));
        good(tree->input({.kind = WidgetInputKind::text, .text = std::move(value)}));
    };
    replace(view->hex_field(), "00FF00"); good(view->apply_button().activate());
    require(editor->snapshot().color == green && !picker->active() && accepts.size() == 1 &&
            near(accepts.front().green, 1) && edits.size() == 1 &&
            take(view->options()).configuration.recent_colors.front() == green,
            "Apply did not commit the focused field and remember its accepted color");
    good(view->recent(1).activate());
    require(editor->snapshot().color == blue && picker->active() && edits.size() == 2,
            "Recent color did not enter the shared edit session");
    good(view->cancel_button().activate());
    require(editor->snapshot().color == green && !picker->active() && edits.size() == 3 && canceled == 1,
            "Cancel did not restore and report the prior color");
    good(view->eyedropper_button().activate()); require(eyedropper == 1, "Eyedropper did not notify its owner");
    replace(view->hex_field(), "FF0000"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::escape}));
    require(editor->snapshot().color == green && !picker->active() && canceled == 2,
            "Escape did not cancel a focused pending color edit");
    auto changed = take(view->options()); changed.configuration.mode = lvui::ColorPickerMode::hue_saturation;
    changed.configuration.width = 480; good(view->set_options(changed)); root = take(view->root().snapshot());
    require(near(root.size.width, 480) && near(root.size.height, 597) && take(view->rail().interaction()).visible &&
            take(view->channel_field(0).interaction()).visible && !take(view->channel_field(1).interaction()).visible,
            "Changing picker mode did not retain and relayout its child controls");
    (void)take(tree->drawing_snapshot({600, 700}));
    const auto surface = take(view->surface().snapshot()).bounds; const auto edits_before = edits.size();
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = surface.x + surface.width / 2, .y = surface.y + surface.height / 2}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = surface.x + surface.width / 2, .y = surface.y + surface.height / 2}));
    require(picker->active() && edits.size() > edits_before && editor->snapshot().color != green,
            "Picker surface did not edit its shared session");
    const auto before_stage = picker->configuration();
    auto staged = changed; staged.configuration.mode = lvui::ColorPickerMode::rgb; staged.configuration.width = 320;
    auto failed_stage = host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto candidate = lvui::native::NativeColorPicker::create(builder, {}, "candidate", staged);
        if (!candidate) return std::unexpected(candidate.error());
        return ii::core::fail(ii::core::ErrorCode::failed, "Intentional candidate failure");
    });
    require(!failed_stage && picker->active() && picker->configuration() == before_stage && view->root().snapshot(),
            "An unpublished picker changed or canceled the existing edit session");
    auto replacement_editor = std::make_shared<lvui::ColorSelection>(red);
    auto replacement_picker = std::make_shared<lvui::ColorPicker>(replacement_editor);
    auto replacement_source = std::make_shared<lvui::ColorSelectionSurface>(replacement_editor);
    auto replacement_session = std::make_shared<lvui::ColorPickerSession>(replacement_picker, replacement_source, replacement_source);
    auto replacement = changed; replacement.session = replacement_session;
    require(!view->set_options(replacement) && take(view->options()).session == session,
            "An active picker session was replaced without completion");
    good(view->cancel()); require(editor->snapshot().color == green, "Surface cancellation did not restore the baseline");
    for (const auto mode : {lvui::ColorPickerMode::saturation_brightness, lvui::ColorPickerMode::grayscale,
                            lvui::ColorPickerMode::rgb, lvui::ColorPickerMode::cmyk, lvui::ColorPickerMode::wheel}) {
        auto candidate = take(view->options()); candidate.configuration.mode = mode; candidate.configuration.width = 320;
        good(view->set_options(candidate));
        const auto expected = lvui::color_picker_layout(candidate.configuration);
        require(expected && near(take(view->root().snapshot()).size.height, expected->height) &&
                take(view->rail().interaction()).visible == bool(expected->rail_kind),
                "A picker mode failed to apply its own geometry");
    }
    good(view->set_options(changed));
    auto invalid = changed; invalid.configuration.width = 100;
    require(!view->set_options(invalid) && near(take(view->root().snapshot()).size.width, 480),
            "Invalid picker options changed the committed layout");
    replace(view->hex_field(), "123456"); require(bool(editor->set_color(blue)), "External replacement failed");
    changed.source_revision = std::int64_t(editor->revision()); good(view->set_options(changed));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(editor->snapshot().color == blue && text(view->hex_field()) == "0000FF",
            "A stale pending value replaced an external picker color");
    bool rejected = false;
    std::thread worker([&] { auto result = view->set_options(changed); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Picker accepted a foreign-thread update");
    (void)take(tree->drawing_snapshot({600, 700}));
    replace(view->hex_field(), "AA0000"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(picker->active(), "Picker did not retain its new edit before retirement");
    good(host.clear_content());
    require(!picker->active() && editor->snapshot().color == blue && !view->set_options(changed),
            "Retiring the picker left its session active or reverted a newer baseline");
    tree->close();
}
