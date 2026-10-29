#include <LVUI/NativeHierarchyList.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
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
std::vector<std::string> keys(const lvui::HierarchySnapshot& value) {
    std::vector<std::string> result; for (const auto& entry : value.entries) result.push_back(entry.key); return result;
}
struct RejectEditor final : lvui::HierarchyEditor {
    lvui::HierarchyResult<std::uint64_t> move(const lvui::HierarchyMove&) override {
        return std::unexpected(lvui::HierarchyError{lvui::HierarchyErrorCode::stale, "Rejected move"});
    }
};
struct InvalidRefreshEditor final : lvui::HierarchyEditor {
    std::shared_ptr<lvui::HierarchyMemory> source;
    lvui::HierarchyResult<std::uint64_t> move(const lvui::HierarchyMove& request) override {
        auto moved = source->move(request); if (!moved) return moved;
        auto snapshot = source->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        auto entries = (*snapshot)->entries; entries.front().icon_name = "missing-icon";
        auto updated = source->replace(std::move(entries), *moved); if (!updated) return std::unexpected(updated.error());
        return *moved;
    }
};
struct RetireEditor final : lvui::HierarchyEditor {
    std::shared_ptr<lvui::HierarchyMemory> source; ii::gui::Widget host;
    lvui::HierarchyResult<std::uint64_t> move(const lvui::HierarchyMove& request) override {
        auto result = source->move(request); if (result) good(host.clear_content()); return result;
    }
};
}
int main() {
    using namespace ii::gui;
    auto source = std::make_shared<lvui::HierarchyMemory>(std::vector<lvui::HierarchyEntry>{
        {.key="a", .label="A"}, {.key="a1", .parent="a", .label="A child"},
        {.key="b", .label="B"}, {.key="c", .label="C"}});
    auto controller = std::make_shared<lvui::HierarchyController>(source);
    lvui::native::HierarchyListInput input; input.controller = controller; input.editor = source;
    input.editable = true; input.width = 200; input.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 140}));
    std::optional<lvui::native::NativeHierarchyList> list; std::vector<std::string> activated, moved;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeHierarchyList::create(builder, *column, "tree", input,
            {.activated = [&](const auto& event) { activated.push_back(event.key); },
             .moved = [&](const auto& plan) { moved.push_back(plan.key); }});
        if (!value) return std::unexpected(value.error()); list = *value; return {};
    }));
    (void)take(list->root().snapshot());
    const auto send = [&](WidgetInputKind kind, double x, double y) { return tree->input({.kind=kind, .x=x, .y=y, .buttons=1}); };
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_up, 10, 10));
    require(activated == std::vector<std::string>{"a"} && take(source->snapshot())->revision == 0,
            "A click in an editable hierarchy did not activate without editing");
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 58));
    require(take(source->snapshot())->revision == 0 && take(take(list->row("a")).options()).configuration.drag_preview,
            "A drag preview modified the source or omitted its visual state");
    good(send(WidgetInputKind::pointer_up, 10, 58)); auto snapshot = take(source->snapshot());
    require(snapshot->revision == 1 && keys(*snapshot) == std::vector<std::string>{"b", "c", "a", "a1"} &&
            moved == std::vector<std::string>{"a"} && activated == std::vector<std::string>{"a"} &&
            !take(take(list->row("a")).options()).configuration.drag_preview,
            "Drop did not commit one subtree move or leaked preview state");
    // The chevron remains a distinct action when editable pointer capture is enabled.
    good(send(WidgetInputKind::pointer_down, 184, 50)); good(send(WidgetInputKind::pointer_up, 184, 50));
    require(bool(list->row("a1")) && take(source->snapshot())->revision == 1, "Editable hierarchy captured its chevron");
    // A source replacement during the gesture must cancel its old move.
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    auto changed = take(source->snapshot())->entries; changed[0].label = "Fresh B"; (void)take(source->replace(changed, 1));
    good(send(WidgetInputKind::pointer_up, 10, 38));
    require(take(source->snapshot())->revision == 2 && moved == std::vector<std::string>{"a"} &&
            take(take(list->row("b")).row().action_options()).accessibility_name == "Fresh B",
            "A stale drag committed or failed to refresh the source");
    // Focus loss and disabling editing during capture each cancel the candidate.
    input.source_revision = 2; good(list->set_options(input));
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    good(tree->input({.kind=WidgetInputKind::focus_lost})); good(send(WidgetInputKind::pointer_up, 10, 38));
    require(take(source->snapshot())->revision == 2 && moved.size() == 1,
            "Focus loss did not cancel the active drag");
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    input.editable = false; good(list->set_options(input)); good(send(WidgetInputKind::pointer_up, 10, 38));
    require(take(source->snapshot())->revision == 2 && moved.size() == 1, "Disabling editing did not cancel the drag");
    good(host.clear_content());

    std::vector<lvui::HierarchyEntry> many;
    for (int i = 0; i < 20; ++i) many.push_back({.key=std::to_string(i), .label="Item " + std::to_string(i)});
    auto large = std::make_shared<lvui::HierarchyMemory>(many);
    input.controller = std::make_shared<lvui::HierarchyController>(large); input.editor = large;
    input.source_revision = 0; input.editable = true; moved.clear(); activated.clear();
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeHierarchyList::create(builder, *column, "large", input,
            {.activated = [&](const auto& event) { activated.push_back(event.key); },
             .moved = [&](const auto& plan) { moved.push_back(plan.key); }});
        if (!value) return std::unexpected(value.error()); list = *value; return {};
    }));
    (void)take(list->root().snapshot());
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 95));
    require(take(list->root().snapshot()).scroll_offset == 20 && take(large->snapshot())->revision == 0,
            "Drag edge did not scroll by one row without editing");
    good(send(WidgetInputKind::pointer_up, 10, 95));
    require(take(large->snapshot())->revision == 1 && moved == std::vector<std::string>{"0"} &&
            take(large->snapshot())->entries[5].key == "0" && activated.empty(),
            "Dropping after edge scrolling used an outdated viewport target");
    good(list->root().set_scroll_offset(0)); input.source_revision = 1; input.editor = std::make_shared<RejectEditor>();
    good(list->set_options(input)); good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    require(!send(WidgetInputKind::pointer_up, 10, 38) && take(large->snapshot())->revision == 1 && moved.size() == 1 &&
            !take(take(list->row("1")).options()).configuration.drag_preview,
            "A rejected move changed the source or left its preview active");
    auto invalid_refresh = std::make_shared<InvalidRefreshEditor>(); invalid_refresh->source = large;
    input.editor = invalid_refresh; good(list->set_options(input));
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    require(!send(WidgetInputKind::pointer_up, 10, 38) && take(large->snapshot())->revision == 3 &&
            moved == std::vector<std::string>{"0", "1"} && list->row("1"),
            "A committed move was hidden when refreshing its new view failed");
    auto repaired = take(large->snapshot())->entries; repaired.front().icon_name.clear();
    input.source_revision = std::int64_t(take(large->replace(repaired, 3)));
    auto retiring = std::make_shared<RetireEditor>(); retiring->source = large; retiring->host = host;
    input.editor = retiring; good(list->set_options(input)); good(list->root().set_scroll_offset(0));
    good(send(WidgetInputKind::pointer_down, 10, 10)); good(send(WidgetInputKind::pointer_move, 10, 38));
    good(send(WidgetInputKind::pointer_up, 10, 38));
    require(take(large->snapshot())->revision == 5 && moved.size() == 2 && !list->set_options(input),
            "A retiring editor delivered a stale callback or retained the widget");
    tree->close();
}
