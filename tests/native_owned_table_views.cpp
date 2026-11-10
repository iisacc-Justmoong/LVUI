#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include "table_header_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
using namespace ii::gui;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
bool has_text(const CanvasFrame& frame,std::string_view text){
    for(const auto& value:frame.texts)if(value.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& value:layer.texts)if(value.text==text)return true;
    return false;
}
}
int main(){
    using namespace lvui;using namespace lvui::native;
    std::vector<TableColumn> columns{{"name","Name",120},{"kind","Kind",120},{"size","Size",120}};
    std::vector<TableRow> rows;
    for(int i=0;i<100;++i)rows.push_back({"r"+std::to_string(i),{{"File "+std::to_string(i)},{"Model"},{"100 MB"}},24});
    auto source=std::make_shared<TableMemory>(columns,rows);auto controller=std::make_shared<TableController>(source);
    TableInput input;input.controller=controller;input.appearance.width=240;input.appearance.height=96;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,140}));std::optional<NativeTable> table;
    std::unordered_map<std::string,std::shared_ptr<const TableViewContext>> contexts;bool reject_row=false;
    const auto factory=[&](WidgetBuilder& builder,std::shared_ptr<const TableViewContext> context)->ii::core::Result<void>{
        const auto value=context->get();const auto key=value.header?"h:"+value.column_key:value.row_key+":"+value.column_key;
        if(reject_row&&value.row_key=="r3")return ii::core::fail(ii::core::ErrorCode::failed,"Rejected cell View");
        contexts[key]=context;
        auto label=builder.label({},"label",{.text=(value.header?"Header ":"Cell ")+value.text});if(!label)return std::unexpected(label.error());
        auto subscription=std::make_shared<ii::core::ObservableSubscription>(context->observe([target=*label](const auto& update,std::uint64_t){
            (void)target.set_text((update.header?"Header ":"Cell ")+update.text);
        }));
        return builder.retain(std::move(subscription));
    };
    NativeTableViews views;views.header=factory;views.columns.emplace("name",factory);
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=NativeTable::create(builder,*column,"table",input,{}, {},views);if(!value)return std::unexpected(value.error());table=*value;return {};
    }));
    auto drawing=take(tree->drawing_snapshot({300,140}));
    require(has_text(drawing,"Header Name")&&has_text(drawing,"Cell File 0")&&has_text(drawing,"Model"),"Custom header/cell views or default columns were not drawn");
    auto first=contexts.at("r0:name");require(first->get().row_index==0&&first->get().column_index==0,"Cell context lost its indices");
    reject_row=true;require(!table->scroll().set_scroll_position(0,24),"A failing cell View was accepted");
    if(take(table->scroll().snapshot()).scroll_offset!=0)throw std::runtime_error("Failed scroll was not restored: "+std::to_string(take(table->scroll().snapshot()).scroll_offset));
    require(first->get().row_key=="r0"&&has_text(take(tree->drawing_snapshot({300,140})),"Cell File 0"),"A failed cell View lost the previous visible generation");
    reject_row=false;
    good(table->scroll().set_scroll_position(0,24));
    bool retired=false;try{(void)first->get();}catch(const std::logic_error&){retired=true;}
    if(!(retired&&contexts.contains("r1:name")&&!contexts.contains("r99:name")))throw std::runtime_error("Cell views did not follow the visible viewport: retired="+std::to_string(retired)+" offset="+std::to_string(take(table->scroll().snapshot()).scroll_offset)+" r1="+std::to_string(contexts.contains("r1:name")));
    good(table->scroll().set_scroll_position(0,0));auto replacement=contexts.at("r0:name");require(replacement!=first,"A retired cell generation was reused");
    auto changed=take(source->snapshot())->rows;changed[0].cells[0].text="Renamed";
    input.source_revision=std::int64_t(take(source->replace(columns,changed,0)));good(table->set_options(input));
    require(contexts.at("r0:name")==replacement&&replacement->get().text=="Renamed"&&has_text(take(tree->drawing_snapshot({300,140})),"Cell Renamed"),
        "A visible cell update lost its View session or observable data");
    auto retire_on_change=replacement->observe([&](const auto& value,std::uint64_t){if(value.text=="Retire")good(host.clear_content());});
    changed[0].cells[0].text="Retire";input.source_revision=std::int64_t(take(source->replace(columns,changed,take(source->snapshot())->revision)));
    (void)table->set_options(input);retired=false;try{(void)replacement->get();}catch(const std::logic_error&){retired=true;}
    require(retired&&!table->root().snapshot(),"A context callback could not retire its table safely");
    std::vector<TableColumn> merged_columns{{"a","A",120},{"b","B",120},{"c","C",120}};
    std::vector<TableRow> merged_rows{{"one",{{"Merged"},{"hidden"},{"C"}},24},{"two",{{"hidden"},{"hidden"},{"C"}},24},{"three",{{"A"},{"B"},{"C"}},24}};
    auto merged_source=std::make_shared<TableMemory>(merged_columns,merged_rows);TableMemoryMergeEditor merge(merged_source);
    (void)take(merge.apply({.revision=0,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=2,.column_count=2}}));
    TableInput merged_input;merged_input.controller=std::make_shared<TableController>(merged_source);
    merged_input.appearance.width=120;merged_input.appearance.height=72;std::optional<NativeTable> merged_table;
    std::shared_ptr<const TableViewContext> anchor;NativeTableViews merged_views;
    merged_views.cell=[&](WidgetBuilder& builder,std::shared_ptr<const TableViewContext> context)->ii::core::Result<void>{
        if(context->get().row_key=="one"&&context->get().column_key=="a")anchor=context;
        auto label=builder.label({},"merged-cell",{.text=context->get().text});if(!label)return std::unexpected(label.error());return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"merged-column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=NativeTable::create(builder,*column,"merged",merged_input,{}, {},merged_views);
        if(!value)return std::unexpected(value.error());merged_table=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,140}));require(anchor&&anchor->get().row_span==2&&anchor->get().column_span==2,"Merged View lost its anchor or span");
    auto merged_anchor=anchor;good(merged_table->scroll().set_scroll_position(130,24));
    require(anchor==merged_anchor&&anchor->get().x<0&&anchor->get().y<0,"An offscreen merge anchor lost its visible View session");
    good(host.clear_content());retired=false;try{(void)anchor->get();}catch(const std::logic_error&){retired=true;}
    require(retired,"A merged View survived its table");
    const std::string long_row(1024,'r'),long_column(1024,'c');
    auto long_source=std::make_shared<TableMemory>(std::vector<TableColumn>{{long_column,"Long",120}},
        std::vector<TableRow>{{long_row,{{"Long key"}},24}});
    TableInput long_input;long_input.controller=std::make_shared<TableController>(long_source);long_input.appearance.width=120;long_input.appearance.height=72;
    std::shared_ptr<const TableViewContext> long_context;NativeTableViews long_views;
    long_views.cell=[&](WidgetBuilder& builder,std::shared_ptr<const TableViewContext> context)->ii::core::Result<void>{
        long_context=context;auto label=builder.label({},"long-cell",{.text=context->get().text});if(!label)return std::unexpected(label.error());return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeTable::create(builder,{},"long-table",long_input,{}, {},long_views);
        if(!value)return std::unexpected(value.error());return {};
    }));
    (void)take(tree->drawing_snapshot({300,140}));
    require(long_context&&long_context->get().row_key==long_row&&long_context->get().column_key==long_column,
        "Maximum-length source keys did not produce a visible cell View");
    good(host.clear_content());
    TableHeaderInput header_input;header_input.configuration.columns=columns;
    header_input.configuration.columns.push_back({"late","Late",120});header_input.configuration.columns.push_back({"last","Last",120});
    header_input.configuration.width=120;
    header_input.configuration.interactive=true;std::optional<TableHeaderTestView> header;
    std::shared_ptr<const TableViewContext> header_first;bool fail_kind=false;int clicks=0;
    NativeTableViews header_views;header_views.header=[&](WidgetBuilder& builder,std::shared_ptr<const TableViewContext> context)->ii::core::Result<void>{
        const auto& item=context->get();if(item.column_key=="late"&&fail_kind)return ii::core::fail(ii::core::ErrorCode::failed,"Rejected header View");
        if(item.column_key=="name")header_first=context;
        auto label=builder.label({},"header-label",{.text="Custom "+item.text});if(!label)return std::unexpected(label.error());return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=TableHeaderTestView::create(builder,{},"header",header_input,{.column_clicked=[&](auto,const auto&){++clicks;}},header_views);
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    require(has_text(take(tree->drawing_snapshot({300,140})),"Custom Name"),"Standalone custom header was not drawn");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.buttons=1}));
    require(clicks==1,"A noninteractive header View blocked the column callback");
    fail_kind=true;auto changed_header=header_input;changed_header.offset=120;
    require(!header->set_options(changed_header),"A failing header View was accepted");
    require(header_first->get().column_key=="name"&&has_text(take(tree->drawing_snapshot({300,140})),"Custom Name"),
        "A failed header View replaced the previous generation");
    fail_kind=false;good(header->set_options(changed_header));retired=false;
    try{(void)header_first->get();}catch(const std::logic_error&){retired=true;}
    require(retired&&has_text(take(tree->drawing_snapshot({300,140})),"Custom Kind"),"Header scrolling did not retire the previous key");
    good(host.clear_content());
    header_input.configuration.interactive=true;int button_clicks=0;const auto prior_clicks=clicks;
    NativeTableViews button_views;button_views.header=[&](WidgetBuilder& builder,std::shared_ptr<const TableViewContext>)->ii::core::Result<void>{
        auto button=builder.button({},"header-button",{.text="Action"},[&]{++button_clicks;});
        if(!button)return std::unexpected(button.error());return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=TableHeaderTestView::create(builder,{},"header-with-button",header_input,{.column_clicked=[&](auto,const auto&){++clicks;}},button_views);
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,140}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.buttons=1}));
    require(button_clicks==1&&clicks==prior_clicks,"A custom header button also triggered the header callback");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    if(!(button_clicks==2&&clicks==prior_clicks)){std::string focused;for(const auto& item:take(tree->snapshots()))if(item.focused)focused+=item.id+" ";
        throw std::runtime_error("The custom header button lost keyboard activation; focused="+focused+" button="+std::to_string(button_clicks)+" header="+std::to_string(clicks-prior_clicks));}
    header_input.configuration.interactive=false;good(header->set_options(header_input));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.buttons=1}));
    require(button_clicks==3,"A custom header button was disabled with the header's default interaction");
    header_input.configuration.columns[0].enabled=false;good(header->set_options(header_input));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.buttons=1}));
    require(button_clicks==3,"A custom header button ignored its disabled column");
    good(host.clear_content());tree->close();
}
