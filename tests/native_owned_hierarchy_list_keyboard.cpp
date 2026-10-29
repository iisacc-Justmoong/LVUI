#include <LVUI/NativeHierarchyList.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}
int main() {
    using namespace ii::gui;
    std::vector<lvui::HierarchyEntry> entries;
    for (int i = 0; i < 30; ++i) entries.push_back({.key=std::to_string(i), .label="Item " + std::to_string(i), .id=i,
                                                    .enabled=i != 3, .selectable=i != 4});
    auto source = std::make_shared<lvui::HierarchyMemory>(entries);
    auto controller = std::make_shared<lvui::HierarchyController>(source);
    lvui::native::HierarchyListInput input; input.controller = controller; input.width = 200; input.height = 60;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 100}));
    std::optional<lvui::native::NativeHierarchyList> list; std::vector<std::string> events;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto view = lvui::native::NativeHierarchyList::create(builder, *column, "tree", input,
            {.activated = [&](const auto& event) { events.push_back(event.key); }});
        if (!view) return std::unexpected(view.error()); list = *view; return {};
    }));
    (void)take(list->root().snapshot()); good(take(list->row("0")).row().focus());
    const auto key = [&](WidgetKey value) { good(tree->input({.kind=WidgetInputKind::key_down, .key=value})); };
    key(WidgetKey::down); key(WidgetKey::down); key(WidgetKey::down);
    require(take(controller->selected_key()) == "5" && events == std::vector<std::string>{"1", "2", "5"} &&
            take(list->root().snapshot()).scroll_offset > 0 && take(take(list->row("5")).row().interaction()).focused,
            "Keyboard navigation did not skip inactive rows, reveal, and focus its selection");
    key(WidgetKey::page_down);
    require(take(controller->selected_key()) == "8" && take(take(list->row("8")).row().interaction()).focused,
            "Page navigation did not move by the visible row count");
    key(WidgetKey::home); require(take(controller->selected_key()) == "0" && take(list->root().snapshot()).scroll_offset == 0,
            "Home did not reveal the first selectable row");
    key(WidgetKey::end); require(take(controller->selected_key()) == "29" && take(take(list->row("29")).row().interaction()).focused,
            "End did not reveal the last selectable row");
    require(take(source->snapshot())->revision == 0, "Keyboard navigation modified the hierarchy source");
    input.keyboard_navigation = false; good(list->set_options(input)); const auto previous = events.size();
    key(WidgetKey::up); require(take(controller->selected_key()) == "29" && events.size() == previous,
            "A disabled keyboard navigation handler changed selection");
    input.keyboard_navigation = true; good(list->set_options(input));
    entries[29].label = "Updated"; (void)take(source->replace(entries, 0));
    key(WidgetKey::up); require(take(controller->selected_key()) == "29" && events.size() == previous &&
            take(take(list->row("29")).row().action_options()).accessibility_name == "Updated",
            "A stale keyboard event was delivered instead of refreshing the source");
    key(WidgetKey::up); require(take(controller->selected_key()) == "28" && events.back() == "28",
            "Keyboard navigation did not resume after a source refresh");
    good(host.clear_content());

    auto nested = std::make_shared<lvui::HierarchyMemory>(std::vector<lvui::HierarchyEntry>{
        {.key="p", .label="Parent"}, {.key="c", .parent="p", .label="Child"},
        {.key="g", .parent="c", .label="Grandchild"}, {.key="q", .label="Other"}});
    auto nested_controller = std::make_shared<lvui::HierarchyController>(nested);
    input.controller = nested_controller; input.source_revision = 0; events.clear();
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto view = lvui::native::NativeHierarchyList::create(builder, *column, "nested", input,
            {.activated = [&](const auto& event) { events.push_back(event.key); }});
        if (!view) return std::unexpected(view.error()); list = *view; return {};
    }));
    (void)take(list->root().snapshot()); good(take(list->row("p")).row().focus());
    key(WidgetKey::right); require(list->row("c") && take(nested_controller->selected_key()) == "p", "Right did not expand the parent");
    key(WidgetKey::right); require(take(nested_controller->selected_key()) == "c" && take(take(list->row("c")).row().interaction()).focused,
            "Right did not navigate from an expanded parent to its child");
    key(WidgetKey::right); require(bool(list->row("g")), "Right did not expand the child");
    key(WidgetKey::right); require(take(nested_controller->selected_key()) == "g", "Right did not navigate to the grandchild");
    key(WidgetKey::left); require(take(nested_controller->selected_key()) == "c", "Left did not navigate to the parent");
    key(WidgetKey::left); require(!list->row("g"), "Left did not collapse the focused parent");
    key(WidgetKey::left); require(take(nested_controller->selected_key()) == "p", "Left did not navigate to the outer parent");
    key(WidgetKey::left); require(!list->row("c") && take(nested->snapshot())->revision == 0,
            "Left did not collapse the hierarchy without editing the source");
    good(host.clear_content()); tree->close();
}
