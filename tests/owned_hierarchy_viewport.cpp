#include <LVUI/HierarchyViewport.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    auto projection = std::make_shared<HierarchyProjection>(); projection->source_revision = 17; projection->view_revision = 4;
    for (std::size_t i = 0; i < 1000; ++i) projection->entries.push_back({.entry = {.key = std::to_string(i)}, .source_index = i});
    auto source = take(HierarchyViewportSource::create(projection));
    HierarchyViewportRequest request{.width = 200, .height = 100, .offset = 1000, .row_height = 20, .overscan = 2, .movement = 1};
    auto frame = take(source->frame(request));
    require(frame.rows.size() == 9 && frame.rows.front().index == 48 && frame.rows.back().index == 56 &&
            frame.anchor.key == "50" && frame.content_height == 20000 && frame.source_revision == 17,
            "Hierarchy viewport did not limit rows to the visible range and overscan");
    auto inserted = std::make_shared<HierarchyProjection>(*projection);
    inserted->entries.insert(inserted->entries.begin(), {.entry = {.key = "new"}, .source_index = 0});
    for (std::size_t i = 0; i < inserted->entries.size(); ++i) inserted->entries[i].source_index = i;
    inserted->source_revision = 18; auto next = take(HierarchyViewportSource::create(inserted));
    request.anchor = frame.anchor; auto anchored = take(next->frame(request));
    require(anchored.offset == 1020 && anchored.anchor.key == "50", "Hierarchy viewport lost its stable scroll anchor after insertion");
    request.movement = 2; auto moved = take(next->frame(request));
    require(moved.offset == 1000 && moved.anchor.key == "49", "A stale anchor overrode user scrolling");
    request.reveal_key = "900"; auto revealed = take(next->frame(request));
    require(revealed.revealed_key == "900" && revealed.offset == 17940 &&
            revealed.rows.back().index >= 901, "Hierarchy viewport did not reveal the requested row");
    request.width = 0; require(take(next->frame(request)).rows.empty(), "A zero-width viewport mounted rows");
    request.width = std::numeric_limits<double>::quiet_NaN(); require(!next->frame(request), "Nonfinite viewport geometry was accepted");
    auto duplicate = std::make_shared<HierarchyProjection>(*projection); duplicate->entries[1].entry.key = "0";
    require(!HierarchyViewportSource::create(duplicate), "Duplicate visible keys were accepted");
    projection->entries[50].entry.key = "changed";
    require(source->find("50") && source->projection().entries[50].entry.key == "50", "Viewport did not retain its immutable source snapshot");
    require(projection->source_revision == 17 && projection->view_revision == 4, "Viewport navigation changed the source projection");
}
