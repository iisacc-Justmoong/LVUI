#include "NativeOwnedColorPicker.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (item == nodes.end()) throw std::runtime_error("Generated color picker did not mount");
    return take(tree->find(item->id));
}
std::string text(ii::gui::Widget field) {
    const auto snapshot = take(field.snapshot()); return take(field.text_content({0, snapshot.text_length})).text;
}
}

int main() {
    using View = lvui_generated::NativeOwnedColorPicker;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {900, 700}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto root = find(tree, "/picker"), field = find(tree, "/picker.values.hex"), apply = find(tree, "/picker.apply");
    require(text(field) == "FF0000" && !text(find(tree, "/default-picker.values.hex")).empty(),
            "Generated pickers did not mount their injected and owned models");
    good(field.focus()); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::select_all}));
    good(tree->input({.kind = WidgetInputKind::text, .text = "00FF00"})); good(apply.activate());
    auto& backend = scope->backend_0;
    require(backend.edits == 1 && backend.accepts == 1 && backend.accepted_color.green == 1 &&
            backend.editor->snapshot().color == lvui::StyleColor{0, 1, 0, .5} &&
            take(find(tree, "/picker.recent-0").interaction()).visible,
            "Generated picker lost its edit, accepted color or local recent color");
    backend.mode.set(lvui::ColorPickerMode::cmyk);
    auto graph = scope->native_lifetime.lock(); require(bool(graph) && take(graph->failures()).empty(), "Picker mode update failed");
    require(std::abs(take(root.snapshot()).size.height - 439) < 1e-6 &&
            take(find(tree, "/picker.recent-0").interaction()).visible,
            "Reactive picker mode lost its geometry or recent color");
    backend.width.set(319);
    require(!take(graph->failures()).empty() && std::abs(take(root.snapshot()).size.width - 320) < 1e-6,
            "Invalid reactive picker width replaced its prior layout");
    backend.width.set(320); require(take(graph->failures()).empty(), "Picker width did not recover");
    backend.model.set({}); require(!take(graph->failures()).empty(), "A null picker session replaced the active view");
    backend.model.set(backend.owner); require(take(graph->failures()).empty(), "Picker session did not recover");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !root.snapshot(), "Generated picker retained its activation");
    tree->close();
}
