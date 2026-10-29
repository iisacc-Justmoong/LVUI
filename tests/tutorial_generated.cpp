#include "Hello.generated.h"
#include "Inventory.generated.h"
#include "ReadingList.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) throw std::runtime_error(value.error().message);
}
using Tree = ii::gui::detail::WidgetState;
std::shared_ptr<Tree> tree() {
    return std::make_shared<Tree>([](ii::runtime::Task task) { task(); });
}
ii::gui::Widget find(const std::shared_ptr<Tree>& state, std::string_view suffix) {
    const auto nodes = take(state->snapshots());
    const auto found = std::ranges::find_if(nodes, [suffix](const auto& node) {
        return node.id.ends_with(suffix);
    });
    require(found != nodes.end(), "Expected tutorial widget is missing");
    return take(state->find(found->id));
}
bool has_text(const ii::gui::CanvasFrame& frame, std::string_view text) {
    for (const auto& item : frame.texts) if (item.text == text) return true;
    for (const auto& layer : frame.overlays)
        for (const auto& item : layer.texts) if (item.text == text) return true;
    return false;
}
bool has_widget_text(const std::shared_ptr<Tree>& state, std::string_view text) {
    const auto nodes = take(state->snapshots());
    return std::ranges::any_of(nodes, [text](const auto& node) { return node.text == text; });
}

void hello() {
    using View = lvui_generated::Hello;
    auto state = tree();
    auto host = take(state->content_host({}, "host", {640, 400}));
    good(host.set_content([&](auto& builder) { return View::build(builder); }));
    require(take(find(state, "/title").snapshot()).text == "Hello, LVUI", "Hello label did not mount");
    const auto progress = take(find(state, "/progress").snapshot());
    require(progress.accessibility_semantics && progress.accessibility_semantics->range &&
            std::abs(progress.accessibility_semantics->range->value - .65) < 0.0001,
            "Progress value was not compiled");
    good(host.clear_content());
    state->close();
}

void reading_list() {
    using View = lvui_generated::ReadingList;
    auto state = tree();
    auto host = take(state->content_host({}, "host", {680, 500}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    require(scope->backend_0.items.get().size() == 2 && has_widget_text(state, "Learn the basics"),
            "Initial repeated list was not mounted");
    auto draft = find(state, "/draft");
    good(draft.focus());
    good(state->input({.kind=ii::gui::WidgetInputKind::text, .text="Native example"}));
    require(scope->backend_0.draft.get() == "Native example", "Input did not update the backend");
    good(find(state, "/add").activate());
    require(scope->backend_0.items.get().size() == 3 && scope->backend_0.draft.get().empty() &&
            has_widget_text(state, "Native example"), "Add did not update the repeated view");
    good(find(state, "/clear").activate());
    require(scope->backend_0.items.get().empty() && !scope->backend_0.hasItems.get() &&
            take(find(state, "/empty").snapshot()).text == "No items yet.",
            "Clear did not show the empty branch");
    good(draft.focus());
    good(state->input({.kind=ii::gui::WidgetInputKind::text, .text="One more"}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.hasItems.get() && scope->backend_0.items.get().size() == 1 &&
            has_widget_text(state, "One more"), "Submitting to an empty list did not restore its rows");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    require(weak.expired() && !draft.snapshot(), "Reading list activation was retained after removal");
    state->close();
}

void inventory() {
    using View = lvui_generated::Inventory;
    auto state = tree();
    auto host = take(state->content_host({}, "host", {720, 500}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    auto table = find(state, "/inventory");
    require(has_text(take(state->drawing_snapshot({720, 500})), "Notebook"), "Inventory data was not drawn");
    const auto bounds = take(table.snapshot()).bounds;
    const auto send = [&](ii::gui::WidgetInputKind kind, double x, double y) {
        good(state->input({.kind=kind, .x=x, .y=y, .buttons=1}));
    };
    send(ii::gui::WidgetInputKind::pointer_down, bounds.x + 25, bounds.y + 34);
    send(ii::gui::WidgetInputKind::pointer_up, bounds.x + 25, bounds.y + 34);
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::select_all}));
    good(state->input({.kind=ii::gui::WidgetInputKind::text, .text="Sketchbook"}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    const auto snapshot = scope->backend_0.data->snapshot();
    require(snapshot && (*snapshot)->rows[0].cells[0].text == "Sketchbook" &&
            scope->backend_0.revision.get() == 1 &&
            has_text(take(state->drawing_snapshot({720, 500})), "Sketchbook"),
            "Inventory edit did not commit and refresh");
    send(ii::gui::WidgetInputKind::pointer_down, bounds.x + 25, bounds.y + 10);
    send(ii::gui::WidgetInputKind::pointer_up, bounds.x + 25, bounds.y + 10);
    const auto sorted = scope->backend_0.data->snapshot();
    require(sorted && (*sorted)->rows[0].cells[0].text == "Camera" &&
            scope->backend_0.revision.get() == 2, "Inventory heading did not sort and refresh");
    send(ii::gui::WidgetInputKind::pointer_down, bounds.x + 25, bounds.y + 34);
    send(ii::gui::WidgetInputKind::pointer_up, bounds.x + 25, bounds.y + 34);
    auto clipboard = std::make_shared<const ii::gui::ClipboardContent>(
        ii::gui::ClipboardContent{.text="Lens\tEquipment"});
    good(state->input({.kind=ii::gui::WidgetInputKind::text, .content=clipboard}));
    const auto pasted = scope->backend_0.data->snapshot();
    require(pasted && (*pasted)->rows[0].cells[0].text == "Lens" &&
            (*pasted)->rows[0].cells[1].text == "Equipment" &&
            scope->backend_0.revision.get() == 3, "Inventory paste did not commit and refresh");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    require(weak.expired() && !table.snapshot(), "Inventory activation was retained after removal");
    state->close();
}
}

int main() {
    hello();
    reading_list();
    inventory();
}
