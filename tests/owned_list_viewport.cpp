#include <LVUI/ListViewport.h>
#include <iiGUI/ListViewport.h>
namespace lvui {using ListViewportSource=ListViewportBinding<ii::gui::BasicListViewportSource>;}

#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
}
int main(){
    using namespace lvui;
    std::vector<ListEntry> entries;for(int i=0;i<10000;++i)entries.push_back({.key=std::to_string(i),.label="Item "+std::to_string(i)});
    auto memory=take(ListMemory::create(std::move(entries)));auto original=take(memory->snapshot());
    auto source=take(ListViewportSource::create(original));
    auto variable=take(ListViewportSource::create(original,{{"0",35},{"1",60},{"3",40}}));
    require(variable.content_extent(20)==200075&&variable.row_offset(4,20)==155&&variable.row_extent(1,20)==60&&
            variable.row_at(34.9,20)==0&&variable.row_at(35,20)==1&&variable.row_at(95,20)==2,
            "Variable list extents or their row boundaries were incorrect");
    ListViewportRequest varying{.width=240,.height=50,.offset=45,.row_height=20,.overscan=1};
    auto variable_frame=take(variable.frame(varying));
    require(variable_frame.content_height==200075&&variable_frame.anchor.key=="1"&&variable_frame.anchor.within==10&&
            variable_frame.rows.size()==3&&variable_frame.rows[0].index==0&&variable_frame.rows[0].y==-45&&
            variable_frame.rows[1].height==60&&variable_frame.rows[2].index==2,
            "Variable list viewport did not materialize the correct rows");
    require(!ListViewportSource::create(original,{{"absent",20}})&&
            !ListViewportSource::create(original,{{"0",std::numeric_limits<double>::quiet_NaN()}})&&
            !ListViewportSource::create(original,{{"0",8193}}),"Invalid variable list extents were accepted");
    ListViewportRequest request{.width=240,.height=100,.offset=100005,.row_height=20,.overscan=1};
    auto frame=take(source.frame(request));
    require(frame.source_revision==0 && frame.content_height==200000 && frame.rows.size()<=8 &&
            frame.rows.front().index==4999 && frame.anchor.key=="5000" && frame.anchor.within==5,
            "List viewport materialized unrelated rows or lost its visible anchor");
    require(take(memory->apply({.revision=0,.kind=ListEditKind::insert,.before="0",.item={.key="new",.label="New"}}))==1,
            "List viewport setup insertion failed");
    auto updated=take(ListViewportSource::create(take(memory->snapshot())));request.anchor=frame.anchor;
    auto variable_updated=take(ListViewportSource::create(take(memory->snapshot()),{{"0",35},{"1",60},{"3",40}}));
    varying.anchor=variable_frame.anchor;
    auto variable_anchored=take(variable_updated.frame(varying));
    require(variable_anchored.offset==65&&variable_anchored.anchor.key=="1"&&variable_anchored.anchor.within==10,
            "Variable row insertion did not preserve the visible key and position");
    varying.anchor={};varying.offset=0;varying.reveal_key="3";varying.reveal=ListReveal::end;
    auto variable_revealed=take(variable_updated.frame(varying));
    require(variable_revealed.offset==125&&variable_revealed.revealed_key=="3",
            "Variable row reveal ignored preceding or target heights");
    auto anchored=take(updated.frame(request));
    require(anchored.offset==100025 && anchored.anchor.key=="5000" && anchored.anchor.within==5,
            "List insertion did not preserve the visible key and position");
    request.anchor.within=1000;auto bounded=take(updated.frame(request));
    require(bounded.anchor.key=="5000" && bounded.anchor.within<request.row_height,
            "An oversized anchor offset moved to a different list row");
    request.anchor=frame.anchor;
    request.movement=1;auto moved=take(updated.frame(request));
    require(moved.offset==100005 && moved.anchor.key=="4999", "User scrolling did not supersede the old list anchor");
    request.anchor={};request.offset=0;request.reveal_key="9999";request.reveal=ListReveal::end;
    auto last=take(updated.frame(request));
    require(last.rows.back().index==10000 && last.offset==200020-100 && last.revealed_key=="9999",
            "List reveal did not clamp the final row into view");
    request.reveal_key="absent";require(!updated.frame(request),"A missing list reveal key was accepted");
    request.reveal_key.clear();request.overscan=129;require(!updated.frame(request),"Excessive list overscan was accepted");
    request.overscan=0;request.height=std::numeric_limits<double>::quiet_NaN();
    require(!updated.frame(request) && original->revision==0 && original->entries.front().key=="0",
            "Invalid list viewport input changed the original snapshot");
}
