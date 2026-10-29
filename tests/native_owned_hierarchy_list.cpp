#include <LVUI/NativeHierarchyList.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
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
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}

int main() {
    using namespace ii::gui;
    std::vector<lvui::HierarchyEntry> entries{{.key="0", .label="Item 0"}, {.key="0a", .parent="0", .label="Child"}};
    for (int i = 1; i < 1000; ++i) entries.push_back({.key=std::to_string(i), .label="Item " + std::to_string(i)});
    auto model = std::make_shared<lvui::HierarchyMemory>(entries);
    auto controller = std::make_shared<lvui::HierarchyController>(model);
    lvui::native::HierarchyListInput input; input.controller = controller; input.width = 240; input.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 180}));
    std::optional<lvui::native::NativeHierarchyList> list; std::vector<std::string> activated, expanded; bool remove_on_activate = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading}); if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeHierarchyList::create(builder, *column, "list", input,
            {.activated = [&](const auto& event) { activated.push_back(event.key); if (remove_on_activate) good(host.clear_content()); },
             .expansion_changed = [&](std::string key, bool value) { expanded.push_back(key + (value ? "+" : "-")); }});
        if (!value) return std::unexpected(value.error()); list = *value; return {};
    }));
    (void)take(tree->drawing_snapshot({300, 180}));
    (void)take(list->root().snapshot());
    auto mounted = take(list->mounted_keys());
    require(!mounted.empty() && mounted.size() <= 7 && mounted.front() == "0" && !list->row("500"),
            "Hierarchy list mounted the entire model instead of its viewport");
    auto first = take(list->row("0")); good(first.row().activate());
    require(activated == std::vector<std::string>{"0"} && take(controller->selected_key()) == "0" &&
            take(model->snapshot())->revision == 0, "Hierarchy selection modified the document or lost its event");
    good(first.chevron().activate());
    require(expanded == std::vector<std::string>{"0+"} && list->row("0a") &&
            activated == std::vector<std::string>{"0", "0"} && take(model->snapshot())->revision == 0,
            "Hierarchy expansion did not mount its child or preserve document state");
    auto retained = take(list->row("2")); auto placement = take(retained.root().placement()).scope.lock();
    good(list->root().set_scroll_offset(20));
    require(retained.root().snapshot() && take(take(list->row("2")).root().placement()).scope.lock() == placement,
            "A retained visible hierarchy row was recreated during scroll");
    good(list->root().set_scroll_offset(15000)); mounted = take(list->mounted_keys());
    require(mounted.size() <= 7 && !retained.root().snapshot() && take(model->snapshot())->revision == 0,
            "Scrolling did not retire offscreen rows or changed the document");
    const auto visible_key = mounted[mounted.size() / 2]; auto before = take(list->row(visible_key));
    const auto stable = take(before.root().placement()).scope.lock();
    auto changed = entries;
    const auto found = std::ranges::find_if(changed, [&](const auto& entry) { return entry.key == visible_key; });
    require(found != changed.end(), "Visible key did not belong to the source"); found->label = "Renamed";
    input.source_revision = std::int64_t(take(model->replace(changed, 0))); good(list->set_options(input));
    auto after = take(list->row(visible_key));
    require(take(after.row().action_options()).accessibility_name == "Renamed" && take(after.root().placement()).scope.lock() == stable,
            "Model refresh rebuilt a retained row or lost its changed label");
    found->icon_name = "missing-icon";
    input.source_revision = std::int64_t(take(model->replace(changed, 1)));
    require(!list->set_options(input) && take(take(list->row(visible_key)).row().action_options()).accessibility_name == "Renamed",
            "A missing icon replaced the last valid hierarchy view");
    found->icon_name.clear(); input.source_revision = std::int64_t(take(model->replace(changed, 2))); good(list->set_options(input));
    require(!take(list->failure()), "Hierarchy list did not recover from a failed candidate");
    const auto offset_before_insert = take(list->root().snapshot()).scroll_offset;
    changed.insert(changed.begin(), {.key="inserted", .label="Inserted"});
    input.source_revision = std::int64_t(take(model->replace(changed, 3))); good(list->set_options(input));
    require(take(list->root().snapshot()).scroll_offset == offset_before_insert + input.row_height && list->row(visible_key),
            "Source insertion did not preserve the visible anchor");
    auto stale = take(list->row(visible_key));
    auto fresh = std::ranges::find_if(changed, [&](const auto& entry) { return entry.key == visible_key; });
    fresh->label = "Fresh source"; (void)take(model->replace(changed, 4));
    const auto event_count = activated.size(); good(stale.row().activate());
    require(activated.size() == event_count && take(take(list->row(visible_key)).row().action_options()).accessibility_name == "Fresh source",
            "A stale hierarchy action was delivered or failed to refresh the source");
    good(take(list->row(visible_key)).row().activate());
    require(activated.size() == event_count + 1 && activated.back() == visible_key && take(model->snapshot())->revision == 5,
            "A refreshed hierarchy action was not delivered without mutating the source");
    input.source_revision = 5;
    bool rejected = false;
    std::thread worker([&] { auto result = list->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Hierarchy list accepted a foreign-thread update");
    remove_on_activate = true; auto last = take(list->row(visible_key)); good(last.row().activate());
    require(!list->set_options(input), "Hierarchy list survived removal during activation");
    tree->close();
}
