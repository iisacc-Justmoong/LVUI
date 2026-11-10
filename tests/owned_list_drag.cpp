#include <LVUI/ListDrag.h>
#include <iiGUI/ListViewport.h>
#include <iiGUI/ListDrag.h>
namespace lvui {using ListViewportSource=ListViewportBinding<ii::gui::BasicListViewportSource>;using ListDragViewport=ListDragViewportValue<ListViewportSource>;using ListDrag=ListDragBinding<ii::gui::BasicListDrag,ListViewportSource>;}

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
}
int main(){
    using namespace lvui;
    auto model=take(ListMemory::create({{.key="a",.label="A"},{.key="b",.label="B"},
        {.key="c",.label="C"},{.key="disabled",.label="Disabled",.enabled=false},
        {.key="fixed",.label="Fixed",.draggable=false}}));
    auto snapshot=take(model->snapshot());
    auto source=std::make_shared<ListViewportSource>(take(ListViewportSource::create(snapshot,{{"a",40}})));
    ListDragViewport viewport{.source=source,.width=200,.height=140,.offset=0,.row_height=20};
    auto unrelated=std::make_shared<ListSnapshot>(*snapshot);unrelated->entries[0].key="other";
    auto mismatch=take(ListDrag::begin(snapshot,"b",{10,50},0));auto wrong_viewport=viewport;
    wrong_viewport.source=std::make_shared<ListViewportSource>(take(ListViewportSource::create(unrelated)));
    auto mismatched=mismatch.update({10,10},wrong_viewport);
    require(!mismatched&&mismatched.error().code==ListErrorCode::stale&&!mismatch.finish(),
        "A different snapshot with the same revision was accepted for a drag");
    auto drag=take(ListDrag::begin(snapshot,"b",{10,50}));
    auto idle=take(drag.update({11,52},viewport));
    require(!idle.dragging&&!idle.proposal&&take(model->snapshot())->revision==0,"A short pointer movement started or committed a drag");
    auto unchanged=take(drag.update({10,62},viewport));
    require(unchanged.dragging&&!unchanged.proposal,"A no-op position produced a move");
    auto after=take(drag.update({10,78},viewport));
    require(after.proposal&&after.proposal->request.key=="b"&&after.proposal->request.before=="disabled"&&
        after.proposal->from==1&&after.proposal->to==2&&after.proposal->target=="c"&&after.proposal->after&&
        take(model->snapshot())->revision==0,"A variable-height drag did not plan the correct after position");
    require(!take(drag.update({10,90},viewport)).proposal,"A disabled drop target was accepted");
    auto end=take(drag.update({10,125},viewport));
    require(end.proposal&&end.proposal->request.before.empty()&&end.proposal->to==4,
        "Trailing empty space did not offer a move to the end");
    auto bad=viewport;bad.width=-1;
    require(!drag.update({10,50},bad)&&!take(drag.finish()),"Invalid geometry retained a stale move");
    auto before=take(ListDrag::begin(snapshot,"b",{10,50},0));
    auto top=take(before.update({10,10},viewport));
    require(top.proposal&&top.proposal->request.before=="a"&&top.proposal->from==1&&top.proposal->to==0&&
        top.proposal->target=="a"&&!top.proposal->after,"Before placement used the wrong index");
    auto committed=take(before.finish());require(committed&&take(model->apply(committed->request))==1&&!before.finish(),
        "A drag did not produce one revision-checked move");
    require(snapshot->entries[0].key=="a"&&take(model->snapshot())->entries[0].key=="b",
        "A committed move mutated an older snapshot");
    auto stale=take(ListDrag::begin(snapshot,"b",{10,50},0));
    auto next=std::make_shared<ListViewportSource>(take(ListViewportSource::create(take(model->snapshot()))));
    viewport.source=next;auto stale_result=stale.update({10,10},viewport);
    require(!stale_result&&stale_result.error().code==ListErrorCode::stale&&!stale.finish(),"A stale drag retained a candidate");
    auto fresh=take(model->snapshot());auto canceled=take(ListDrag::begin(fresh,"b",{10,10},0));canceled.cancel();
    require(!canceled.finish()&&!ListDrag::begin(fresh,"disabled",{0,0})&&!ListDrag::begin(fresh,"fixed",{0,0})&&
        !ListDrag::begin(fresh,"missing",{0,0})&&!ListDrag::begin(fresh,"a",{NAN,0}),"Invalid or canceled drags were accepted");
    viewport.height=30;viewport.offset=20;
    auto scrolling=take(ListDrag::begin(fresh,"b",{0,10},0));
    require(take(scrolling.update({10,1},viewport)).scroll_direction==-1&&
        take(scrolling.update({10,29},viewport)).scroll_direction==1,"Drag did not request edge scrolling");
}
