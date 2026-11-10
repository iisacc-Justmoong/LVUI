#include <LVUI/HierarchyViewport.h>
#include <iiGUI/HierarchyViewport.h>
#include <iiGUI/HierarchyDrag.h>
namespace lvui {using HierarchyViewportSource=HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>;}

#include <algorithm>
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

    auto variable=std::make_shared<HierarchyProjection>();
    for(std::size_t i=0;i<5;++i)variable->entries.push_back({.entry={.key=std::to_string(i)},.source_index=i});
    variable->entries[1].row_extent=40.;variable->entries[3].row_extent=10.;
    auto varied=take(HierarchyViewportSource::create(variable));
    HierarchyViewportRequest window{.width=200,.height=30,.offset=25,.row_height=20,.overscan=0,.movement=4};
    auto rows=take(varied->frame(window));
    require(rows.content_height==110&&rows.rows.size()==1&&rows.rows[0].index==1&&
            rows.rows[0].y==-5&&rows.rows[0].height==40&&rows.anchor.key=="1"&&rows.anchor.within==5,
            "Variable-height viewport did not locate the visible row");
    window.anchor=rows.anchor;variable->entries[0].row_extent=30.;
    auto resized=take(HierarchyViewportSource::create(variable));auto preserved=take(resized->frame(window));
    require(preserved.offset==35&&preserved.anchor.key=="1"&&preserved.anchor.within==5,
            "Resizing an earlier row lost the viewport anchor");
    window.movement=5;window.reveal_key="3";auto reveal=take(resized->frame(window));
    require(reveal.offset==70&&reveal.rows.back().index==3&&reveal.rows.back().height==10,
            "Variable-height reveal used a fixed row offset");
    variable->entries[1].row_extent=std::numeric_limits<double>::infinity();
    require(!HierarchyViewportSource::create(variable),"Invalid projected row height was accepted");

    auto distant=std::make_shared<HierarchyProjection>();
    for(std::size_t i=0;i<200;++i)distant->entries.push_back({.entry={.key=std::to_string(i)},.source_index=i});
    distant->entries[100].row_extent=1.;auto distant_source=take(HierarchyViewportSource::create(distant));
    HierarchyViewportRequest shrunk{.width=100,.height=1,.row_height=8192,.overscan=0,.movement=3,
        .anchor={.key="100",.index=100,.within=4000,.movement=3}};
    auto clamped=take(distant_source->frame(shrunk));
    require(clamped.anchor.key=="100"&&clamped.anchor.within<1.,
            "A distant shortened row rounded its retained anchor into the next row");

    auto mixed=std::make_shared<HierarchyProjection>();
    for(std::size_t i=0;i<40;++i){
        HierarchyVisibleEntry row{.entry={.key=std::to_string(i)},.source_index=i};
        if(i%3==0)row.row_extent=double(1+(i*37)%83);
        mixed->entries.push_back(std::move(row));
    }
    auto indexed=take(HierarchyViewportSource::create(mixed));
    for(double fallback:{5.,20.,45.}){
        std::vector<double> starts{0};
        for(const auto& row:mixed->entries)starts.push_back(starts.back()+row.row_extent.value_or(fallback));
        require(indexed->content_extent(fallback)==starts.back()&&!indexed->row_at(starts.back(),fallback),
                "Variable extent total or end boundary is incorrect");
        for(double height:{1.,19.,68.,800.})for(double requested=0;requested<starts.back()+100;requested+=53){
            HierarchyViewportRequest sample{.width=200,.height=height,.offset=requested,.row_height=fallback,.overscan=2};
            auto actual=take(indexed->frame(sample));
            const auto offset=std::clamp(requested,0.,std::max(0.,starts.back()-height));
            std::size_t first=0;while(first+1<starts.size()-1&&starts[first+1]<=offset)++first;
            std::size_t end=first+1;while(end<starts.size()-1&&starts[end]<offset+height)++end;
            const auto expected_first=first>2?first-2:0,expected_end=std::min(mixed->entries.size(),end+2);
            require(actual.offset==offset&&actual.rows.size()==expected_end-expected_first&&actual.anchor.index==first,
                    "Mixed row viewport range disagrees with sequential layout");
            for(std::size_t i=expected_first;i<expected_end;++i){const auto& row=actual.rows[i-expected_first];
                require(row.index==i&&row.y==starts[i]-offset&&row.height==starts[i+1]-starts[i],
                        "Mixed row geometry disagrees with sequential layout");}
        }
    }
}
