#include <LVUI/HierarchyDrag.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    auto model = std::make_shared<HierarchyMemory>(std::vector<HierarchyEntry>{
        {.key="a", .label="A"}, {.key="a1", .parent="a", .label="A1"},
        {.key="b", .label="B"}, {.key="c", .label="C"}, {.key="disabled", .label="Disabled", .enabled=false}});
    auto snapshot = take(model->snapshot()); HierarchyController controller(model);
    (void)take(controller.set_expanded("a", true));
    auto projection = take(controller.projection());
    auto source = take(HierarchyViewportSource::create(std::make_shared<const HierarchyProjection>(projection)));
    HierarchyDragViewport viewport{.source=source, .width=200, .height=120, .offset=0, .row_height=20};
    auto drag = take(HierarchyDrag::begin(snapshot, "a", {10, 10}));
    auto idle = take(drag.update({12, 10}, viewport)); require(!idle.dragging && !idle.proposal, "Drag started below the movement threshold");
    auto cycle = take(drag.update({10, 30}, viewport)); require(cycle.dragging && !cycle.proposal, "A subtree could target its own descendant");
    auto child = take(drag.update({10, 50}, viewport));
    require(child.proposal && child.proposal->request.placement == HierarchyPlacement::child &&
            child.proposal->plan.new_parent == "b" && take(model->snapshot())->revision == 0,
            "Drag preview lost its target or edited the source");
    auto after = take(drag.update({10, 78}, viewport));
    require(after.proposal && after.proposal->request.placement == HierarchyPlacement::after && after.proposal->request.target == "c",
            "Drag did not classify the bottom edge as an after placement");
    auto rejected = take(drag.update({10, 90}, viewport)); require(!rejected.proposal, "Disabled drop target was accepted");
    auto root = take(drag.update({10, 110}, viewport));
    require(root.proposal && root.proposal->request.placement == HierarchyPlacement::root_end,
            "Trailing empty space did not offer a root-end move");
    auto invalid_view = viewport; invalid_view.row_height = 0;
    require(!drag.update({10, 110}, invalid_view), "Invalid drag geometry was accepted");
    require(bool(take(drag.update({10, 110}, viewport)).proposal), "Drag did not recover after invalid geometry");
    auto finished = take(drag.finish()); require(finished && take(model->move(finished->request)) == 1 && !drag.finish(),
            "Drag completion was not a single revision-checked edit");
    require(snapshot->entries.front().key == "a" && take(model->snapshot())->entries.back().key == "a1",
            "Moving the subtree changed an older immutable snapshot");
    auto stale = take(HierarchyDrag::begin(snapshot, "a", {0, 0}, 0));
    auto changed = take(controller.projection());
    auto new_source = take(HierarchyViewportSource::create(std::make_shared<const HierarchyProjection>(changed)));
    viewport.source = new_source;
    auto stale_result = stale.update({10, 10}, viewport);
    require(!stale_result && stale_result.error().code == HierarchyErrorCode::stale && !stale.finish(),
            "A stale drag retained a commit candidate");
    auto fresh_snapshot = take(model->snapshot());
    auto invalidated = take(HierarchyDrag::begin(fresh_snapshot, "b", {0, 0}, 0));
    auto candidate = take(invalidated.update({10, 110}, viewport)); require(bool(candidate.proposal), "Test drag did not create a candidate");
    auto invalid_again = viewport; invalid_again.width = -1;
    require(!invalidated.update({10, 110}, invalid_again) && !take(invalidated.finish()),
            "Invalid pointer geometry retained a stale drop candidate");
    auto canceled = take(HierarchyDrag::begin(fresh_snapshot, "b", {0, 0}, 0)); canceled.cancel();
    require(!canceled.finish(), "Canceled drag could finish");
    require(!HierarchyDrag::begin(fresh_snapshot, "disabled", {0, 0}) && !HierarchyDrag::begin(fresh_snapshot, "missing", {0, 0}),
            "Disabled or absent drag source was accepted");
    viewport.height = 30; viewport.offset = 20;
    auto scrolling = take(HierarchyDrag::begin(fresh_snapshot, "b", {0, 10}, 0));
    require(take(scrolling.update({10, 1}, viewport)).scroll_direction == -1 &&
            take(scrolling.update({10, 29}, viewport)).scroll_direction == 1,
            "Drag did not signal viewport edge scrolling");
}
