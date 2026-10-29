#include <LVUI/HierarchyToolbarLayout.h>

#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::HierarchyToolbarResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class Command final : public lvui::HierarchyToolbarCommand {
public:
    void invoke(const lvui::ToolbarButtonId&) const override {}
};
}
int main() {
    using namespace lvui;
    HierarchyToolbar toolbar; auto command = std::make_shared<Command>();
    std::vector<HierarchyToolbarItem> items{{.id=ToolbarButtonId{std::string("draw")}, .icon_name="home"},
        {.id=ToolbarButtonId{std::string("hidden")}, .visible=false, .selected=true},
        {.id=ToolbarButtonId{std::int64_t(8)}, .enabled=false},
        {.id=ToolbarButtonId{std::string("move")}, .icon_name="generaladd", .selected=true, .command=command}};
    require(take(toolbar.set_items(items)), "Toolbar items were not accepted"); auto initial = take(toolbar.snapshot());
    require(initial->active && initial->active->id == ToolbarButtonId{std::string("move")} && initial->active->index == 3,
            "Toolbar did not normalize to a visible enabled selection");
    HierarchyToolbarAppearance appearance; auto layout = hierarchy_toolbar_geometry(*initial, appearance, {0, 0, 0, 0});
    require(layout && layout->width == 200 && layout->height == 26 && layout->slots[1].width == 0 && layout->slots[0].x == 8,
            "Toolbar default layout or hidden slot geometry changed");
    appearance.distribute_spacing = true; auto distributed = hierarchy_toolbar_geometry(*initial, appearance, {0, 0, 0, 0});
    require(distributed && distributed->slots[3].x + distributed->slots[3].width == 192, "Toolbar did not distribute spare spacing");
    appearance.width = 30; auto narrow = hierarchy_toolbar_geometry(*initial, appearance, {0, 0, 0, 0});
    require(narrow && narrow->slots[3].x + narrow->slots[3].width <= 22.0001, "Narrow toolbar slots escaped their bounds");
    require(!take(toolbar.set_items(items)) && take(toolbar.snapshot())->revision == initial->revision, "Unchanged toolbar items published a revision");
    require(!toolbar.activate(ToolbarButtonId{std::string("hidden")}) && !toolbar.activate(ToolbarButtonId{std::int64_t(8)}) &&
            !toolbar.activate(ToolbarButtonId{std::string("missing")}), "Toolbar activated an unavailable item");
    auto draw = take(toolbar.activate(ToolbarButtonId{std::string("draw")}));
    require(draw.changed && draw.selection.index == 0 && !draw.command && !take(toolbar.activate(draw.selection.id)).changed,
            "Toolbar activation did not preserve or report its view selection");
    require(take(toolbar.navigate(HierarchyToolbarDirection::next)) && take(toolbar.snapshot())->active->id == ToolbarButtonId{std::string("move")} &&
            !take(toolbar.navigate(HierarchyToolbarDirection::next)), "Toolbar navigation did not skip disabled or hidden items");
    auto active = take(toolbar.activate(ToolbarButtonId{std::string("move")})); require(active.command == command, "Toolbar command was not retained");
    std::swap(items[0], items[3]); require(take(toolbar.set_items(items)) && take(toolbar.snapshot())->active->index == 0 &&
        initial->active->index == 3, "Toolbar did not retain keyed selection or its immutable old snapshot");
    items[0].visible = false; require(take(toolbar.set_items(items)) && take(toolbar.snapshot())->active->id == ToolbarButtonId{std::string("draw")},
        "Toolbar did not replace a hidden active item");
    auto duplicate = items; duplicate[1].id = duplicate[0].id; auto revision = take(toolbar.snapshot())->revision;
    require(!toolbar.set_items(duplicate) && take(toolbar.snapshot())->revision == revision, "Duplicate toolbar IDs replaced the committed state");
    for (auto& item : items) item.enabled = false; require(take(toolbar.set_items(items)) && !take(toolbar.snapshot())->active &&
        !take(toolbar.navigate(HierarchyToolbarDirection::first)), "A toolbar without available items retained an active tool");
    require(!toolbar.set_items(std::vector<HierarchyToolbarItem>(257)), "Toolbar accepted too many items");
    bool wrong_thread = false; std::thread worker([&] { auto value = toolbar.snapshot(); wrong_thread = !value && value.error().code == HierarchyToolbarErrorCode::wrong_thread; });
    worker.join(); require(wrong_thread, "Toolbar controller accepted a foreign thread");
}
