#include "table_header_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view value){
    for(const auto& run:frame.texts)if(run.text==value)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==value)return true;return false;
}
std::vector<ii::gui::ImageRun> images(const ii::gui::CanvasFrame& frame){
    auto result=frame.images;for(const auto& layer:frame.overlays)result.insert(result.end(),layer.images.begin(),layer.images.end());return result;
}
const ii::gui::TextRun* text(const ii::gui::CanvasFrame& frame,std::string_view value){
    for(const auto& run:frame.texts)if(run.text==value)return &run;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==value)return &run;return nullptr;
}
struct MutableIcons final:lvui::IconSource{
    std::string red,blue;mutable std::uint64_t version=1;mutable std::size_t requests=0;
    mutable std::function<void()> on_resolve;bool churn=false;mutable bool revision_ticks=false;
    [[nodiscard]] lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view name)const override{
        ++requests;if(auto callback=std::exchange(on_resolve,{}))callback();if(churn)++version;
        if(name=="red"||name=="blue")return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{name=="red"?red:blue});
        return std::unexpected(lvui::IconError{lvui::IconErrorCode::not_found,"fixture icon missing"});
    }
    [[nodiscard]] std::uint64_t revision()const noexcept override{if(revision_ticks)++version;return version;}
};
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::TableHeaderInput input;input.configuration.width=300;input.configuration.interactive=true;
    input.configuration.columns={{"a","First",100},{"b","Second",100},{"c","Third",100}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,80}));std::optional<TableHeaderTestView> header;
    std::vector<std::string> clicked;bool retire=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=TableHeaderTestView::create(builder,*column,"header",input,{.column_clicked=[&](std::size_t,const TableColumn& column){
            clicked.push_back(column.key);if(retire)good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    auto root=take(header->root().snapshot());auto drawing=take(tree->drawing_snapshot({350,80}));
    require(root.size.width==300&&root.size.height==25&&contains(drawing,"First")&&contains(drawing,"Third"),"Header sizing or text failed");
    auto initial_icons=images(drawing);
    require(valid_canvas(drawing)&&initial_icons.size()==3&&std::abs(initial_icons[0].bounds.x-root.bounds.x-8)<.01&&
        std::abs(initial_icons[0].bounds.width-18)<.01,"Default column icons were not drawn");
    const std::string red="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\"><rect width=\"18\" height=\"18\" fill=\"#ff0000\"/></svg>";
    const std::string blue="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\"><rect width=\"18\" height=\"18\" fill=\"#0000ff\"/></svg>";
    auto catalog=std::make_shared<MutableIcons>();catalog->red=red;catalog->blue=blue;
    auto styled=input;styled.icon_source=catalog;styled.configuration.default_leading_icon="";
    styled.configuration.column_appearance={{.column="a",.padding=3.,.leading_icon="red",.trailing_icon="blue"}};
    good(header->set_options(styled));drawing=take(tree->drawing_snapshot({350,80}));auto icon_runs=images(drawing);
    auto first=text(drawing,"First");
    require(valid_canvas(drawing)&&icon_runs.size()==2&&icon_runs[0].image!=icon_runs[1].image&&first&&
        std::abs(icon_runs[0].bounds.x-root.bounds.x-3)<.01&&std::abs(first->bounds.x-root.bounds.x-28)<.01&&
        icon_runs[1].bounds.x>=first->bounds.x+first->bounds.width-0.01,"Per-column icons or padding were not drawn correctly");
    auto styled_sort=styled;styled_sort.configuration.sort=TableHeaderSort{"a",TableSortOrder::ascending};
    styled_sort.configuration.columns[0].title="A long column title that must be clipped";
    good(header->set_options(styled_sort));drawing=take(tree->drawing_snapshot({350,80}));auto sorted_icons=images(drawing);
    auto indicator=text(drawing,"↑");require(sorted_icons.size()==2&&indicator&&
        sorted_icons[1].bounds.x+sorted_icons[1].bounds.width<=indicator->bounds.x+.01,
        "The trailing icon overlapped the sort indicator");
    good(header->set_options(styled));
    auto missing=styled;missing.configuration.width=100;missing.configuration.column_appearance.push_back({.column="c",.leading_icon="missing"});
    require(!header->set_options(missing)&&take(header->options())==styled,"Invalid offscreen-capable icon replaced the good header");
    auto no_fallback=styled;no_fallback.configuration.default_leading_icon="generalchevronDown";
    require(!header->set_options(no_fallback)&&take(header->options())==styled,
        "A custom icon source unexpectedly fell back to the built-in catalog");
    auto invalid_revision=styled;invalid_revision.icon_revision=-1;
    require(!header->set_options(invalid_revision)&&take(header->options())==styled,"Negative icon revision replaced the good header");
    const auto original_icon=icon_runs[0].image;const auto requests=catalog->requests;
    styled.icon_revision=1;good(header->set_options(styled));icon_runs=images(take(tree->drawing_snapshot({350,80})));
    require(icon_runs[0].image==original_icon&&catalog->requests==requests,"Unchanged icon source was decoded again");
    catalog->red=blue;++catalog->version;styled.icon_revision=2;good(header->set_options(styled));
    icon_runs=images(take(tree->drawing_snapshot({350,80})));
    require(icon_runs[0].image!=original_icon&&catalog->requests>requests,"Updated icon source did not refresh the image");
    ++catalog->version;auto newer=styled;newer.icon_revision=4;newer.configuration.columns[0].title="Newer";
    catalog->on_resolve=[&]{good(header->set_options(newer));};
    auto stale=styled;stale.icon_revision=3;stale.configuration.columns[0].title="Stale";good(header->set_options(stale));
    require(take(header->options())==newer,"A reentrant icon source let an older candidate overwrite the newer header");
    good(header->set_options(styled));
    auto huge=styled;huge.configuration.columns.clear();huge.configuration.column_appearance.clear();huge.configuration.icon_size=256;
    for(int i=0;i<65;++i){const auto key=std::to_string(i);huge.configuration.columns.push_back({key,"Column",32});
        huge.configuration.column_appearance.push_back({.column=key,.leading_icon="icon"+key});}
    const auto before_budget=catalog->requests;auto limited=header->set_options(huge);
    require(!limited&&limited.error().code==ii::core::ErrorCode::out_of_range&&catalog->requests==before_budget&&take(header->options())==styled,
        "Icon raster budget did not reject atomically before decoding");
    catalog->churn=true;++catalog->version;auto unstable=styled;unstable.icon_revision=5;
    require(!header->set_options(unstable)&&take(header->options())==styled,"An unstable icon source replaced the committed header");
    catalog->churn=false;
    auto no_icons=styled;no_icons.configuration.column_appearance.clear();catalog->revision_ticks=true;
    const auto before_empty=catalog->requests;good(header->set_options(no_icons));
    require(images(take(tree->drawing_snapshot({350,80}))).empty()&&catalog->requests==before_empty,
        "Empty icon slots depended on the icon source");
    auto hidden=styled;hidden.configuration.icon_size=0;hidden.configuration.column_appearance[0].leading_icon="missing";
    const auto before_hidden=catalog->requests;good(header->set_options(hidden));catalog->revision_ticks=false;
    require(images(take(tree->drawing_snapshot({350,80}))).empty()&&catalog->requests==before_hidden,
        "Zero-size icons were decoded or displayed");
    auto short_row=styled;short_row.configuration.row_height=10;good(header->set_options(short_row));
    drawing=take(tree->drawing_snapshot({350,80}));icon_runs=images(drawing);
    require(valid_canvas(drawing)&&icon_runs.size()==2&&std::abs(icon_runs[0].bounds.height-10)<.01&&
        std::abs(icon_runs[0].source.y-2.f/9.f)<.01,"A short row did not vertically crop the icon");
    auto clipped=styled;clipped.configuration.width=100;clipped.offset=15;
    good(header->set_options(clipped));drawing=take(tree->drawing_snapshot({350,80}));icon_runs=images(drawing);
    require(valid_canvas(drawing)&&!icon_runs.empty()&&std::abs(icon_runs[0].bounds.x-root.bounds.x)<.01&&
        std::abs(icon_runs[0].bounds.width-6)<.01&&std::abs(icon_runs[0].source.x-2.f/3.f)<.01,
        "Scrolled header did not crop the leading icon without rescaling it");
    good(header->set_options(input));
    auto sorted=input;sorted.configuration.sort=TableHeaderSort{"b",TableSortOrder::ascending};
    sorted.text.sorted_ascending="Ascending locally";sorted.text.sorted_descending="Descending locally";
    good(header->set_options(sorted));drawing=take(tree->drawing_snapshot({350,80}));
    require(contains(drawing,"↑")&&!contains(drawing,"↓"),"Ascending indicator was not drawn");
    auto headers=take(tree->accessibility_snapshots());
    require(std::ranges::any_of(headers,[](const auto& value){return value.accessibility_name=="Second"&&value.accessibility_description=="Ascending locally";}),
        "Ascending indicator was not exposed to accessibility");
    sorted.configuration.sort->order=TableSortOrder::descending;good(header->set_options(sorted));
    drawing=take(tree->drawing_snapshot({350,80}));headers=take(tree->accessibility_snapshots());
    require(contains(drawing,"↓")&&!contains(drawing,"↑")&&std::ranges::any_of(headers,[](const auto& value){
        return value.accessibility_name=="Second"&&value.accessibility_description=="Descending locally";}),
        "Descending indicator did not update visually and semantically");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    sorted.configuration.palette.current=lvui::figma_defaults::accent_purple_default;
    good(header->set_options(sorted));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(clicked==std::vector<std::string>{"b"}&&take(header->options()).configuration.sort==sorted.configuration.sort,
        "A palette change interrupted header activation or sorting delegation");
    clicked.clear();
    auto bad_sort=sorted;bad_sort.configuration.sort->column="missing";
    require(!header->set_options(bad_sort)&&take(header->options())==sorted,"Invalid sort indicator replaced the good header");
    sorted.configuration.interactive=false;good(header->set_options(sorted));headers=take(tree->accessibility_snapshots());
    require(std::ranges::any_of(headers,[](const auto& value){return value.accessibility_name=="Second"&&
        value.accessibility_description=="Descending locally"&&!value.accessibility_actionable;}),"Read-only sort status was not exposed");
    good(header->set_options(input));drawing=take(tree->drawing_snapshot({350,80}));
    require(!contains(drawing,"↑")&&!contains(drawing,"↓"),"Clearing sort did not remove the indicator");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(clicked==std::vector<std::string>{"b"},"Header pointer did not report its column");
    good(header->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(clicked==std::vector<std::string>({"b","c"}),"Header keyboard navigation failed");
    auto next=input;next.configuration.columns[1].title="Changed";next.configuration.columns[1].enabled=false;
    good(header->set_options(next));drawing=take(tree->drawing_snapshot({350,80}));require(contains(drawing,"Changed"),"Header did not update its content");
    auto invalid=next;invalid.configuration.columns[2].key="a";
    require(!header->set_options(invalid)&&take(header->options())==next,"Invalid columns replaced the good header");
    bool wrong=false;std::thread worker([&]{auto result=header->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Header accepted a foreign-thread update");
    next.configuration.enabled=false;good(header->set_options(next));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    require(clicked.size()==2,"Disabled header delivered an event");
    next.configuration.enabled=true;good(header->set_options(next));retire=true;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    require(clicked.size()==3&&!header->set_options(input),"Retired header accepted an update");
    input.configuration.width=100;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=TableHeaderTestView::create(builder,*column,"scrolled",input,{.column_clicked=[&](std::size_t,const TableColumn& column){clicked.push_back(column.key);}});
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    good(header->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::home}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(header->viewport()).x==100,"Keyboard navigation did not reveal the focused column");
    const auto accessible=take(tree->accessibility_snapshots());
    require(std::ranges::any_of(accessible,[](const auto& value){return value.accessibility_semantics&&
        value.accessibility_semantics->role==AccessibilityRole::column_header&&value.accessibility_name=="Second"&&value.focused;}),
        "Keyboard scrolling did not publish the focused accessible column");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(clicked.back()=="b","Scrolled keyboard activation selected the wrong column");
    good(host.clear_content());
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.absolute_stack({},"positioned",{300,50});if(!stack)return std::unexpected(stack.error());
        auto value=TableHeaderTestView::create(builder,*stack,"positioned-header",styled);if(!value)return std::unexpected(value.error());
        header=*value;return header->root().set_frame({0,0,300,25});
    }));
    (void)take(tree->drawing_snapshot({350,80}));(void)take(header->root().snapshot());
    ++catalog->version;auto layout_newer=styled;layout_newer.icon_revision=7;layout_newer.configuration.columns[0].title="Layout newer";
    catalog->on_resolve=[&]{good(header->set_options(layout_newer));};
    good(header->root().set_frame({0,0,150,25}));(void)take(tree->drawing_snapshot({350,80}));(void)take(header->root().snapshot());
    require(take(header->options())==layout_newer,"Reentrant icon resolution during layout lost the newer options");
    require(take(header->viewport()).width==150,"Reentrant icon resolution during layout lost the actual viewport width");
    auto requested=layout_newer;requested.configuration.width=250;requested.icon_revision=8;
    good(header->set_options(requested));require(take(header->viewport()).width==150,
        "Changing the requested width replaced a fixed parent allocation");
    good(header->root().set_frame({0,0,250,25}));(void)take(tree->drawing_snapshot({350,80}));(void)take(header->root().snapshot());
    requested.configuration.width=200;requested.icon_revision=9;good(header->set_options(requested));
    require(take(header->viewport()).width==250,"A formerly matching fixed allocation was lost when requested width changed");
    good(header->root().set_frame({0,0,180,25}));++catalog->version;
    auto snapshot_winner=requested;snapshot_winner.icon_revision=10;snapshot_winner.configuration.columns[0].title="Snapshot winner";
    catalog->on_resolve=[&]{good(header->set_options(snapshot_winner));};
    auto snapshot_loser=requested;snapshot_loser.icon_revision=11;snapshot_loser.configuration.columns[0].title="Snapshot loser";
    good(header->set_options(snapshot_loser));
    require(take(header->options())==snapshot_winner&&take(header->viewport()).width==180,
        "A layout callback during option inspection lost a newer update");
    ++catalog->version;catalog->on_resolve=[&]{good(host.clear_content());};
    auto retiring=snapshot_winner;retiring.icon_revision=12;good(header->set_options(retiring));
    require(!header->root().snapshot(),"A header retired during icon resolution remained live");tree->close();
}
