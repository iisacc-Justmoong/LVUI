#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& item:frame.texts)if(item.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& item:layer.texts)if(item.text==text)return true;return false;
}
bool crosses_interior(const ii::gui::CanvasFrame& frame){
    const auto cross=[](const auto& panels){for(const auto& panel:panels){const auto& b=panel.bounds;
        if(std::abs(b.x-79)<.1&&b.width<=1.1&&b.y<70&&b.y+b.height>25)return true;
        if(std::abs(b.y-47)<.1&&b.height<=1.1&&b.x<159&&b.x+b.width>1)return true;
    }return false;};
    if(cross(frame.panels))return true;for(const auto& layer:frame.overlays)if(cross(layer.panels))return true;return false;
}
}
int main(){
    using namespace ii::gui;using namespace lvui;
    std::vector<TableColumn> columns;std::vector<TableRow> rows;
    for(int c=0;c<5;++c)columns.push_back({.key="c"+std::to_string(c),.width=80});
    for(int r=0;r<5;++r){TableRow row;row.key="r"+std::to_string(r);
        for(int c=0;c<5;++c)row.cells.push_back({.text=r==0&&c==0?"anchor":std::to_string(r)+"/"+std::to_string(c)});
        rows.push_back(std::move(row));}
    auto source=std::make_shared<TableMemory>(columns,rows);auto merge_editor=std::make_shared<TableMemoryMergeEditor>(source);
    require(take(merge_editor->apply({.revision=0,.action=TableMergeAction::merge,
                                    .range={.row=0,.column=0,.row_count=3,.column_count=3}}))==1,"Initial merge failed");
    auto controller=std::make_shared<TableController>(source);native::TableInput input;
    input.controller=controller;input.editor=source;input.editable=true;input.merge_editor=merge_editor;
    input.appearance.width=160;input.appearance.height=72;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::optional<native::NativeTable> table;int changes=0;
    bool try_nested=false,nested_rejected=false,retire=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});
        if(!column)return std::unexpected(column.error());
        auto view=native::NativeTable::create(builder,*column,"table",input,{.merge_changed=[&](const auto&){
            ++changes;if(try_nested){auto nested=table->apply_merge({.revision=3,.action=TableMergeAction::split,.range={.row=0,.column=0}});
                nested_rejected=!nested&&nested.error().code==ii::core::ErrorCode::busy;}
            if(retire)good(host.clear_content());}});
        if(!view)return std::unexpected(view.error());table=*view;return {};
    }));
    auto drawing=take(tree->drawing_snapshot({200,100}));
    require(has_text(drawing,"anchor")&&!has_text(drawing,"0/1")&&!has_text(drawing,"1/1")&&!crosses_interior(drawing),
            "Merged drawing showed covered cell text or interior grid lines");
    good(table->scroll().set_scroll_position(90,30));auto frame=take(table->frame());
    require(frame->merges.size()==1&&frame->merges[0].x==-90&&frame->merges[0].y==-30,
            "A merged cell disappeared when its anchor scrolled out of view");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=50,.y=34,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=50,.y=34}));
    require(take(controller->selection())->current==TablePosition{"r0","c0"},"Clicking a covered cell did not select the anchor");
    good(table->begin_edit());auto field=take(table->editor());
    require(take(field.snapshot()).text=="anchor","An offscreen anchor could not be edited through its visible merge");
    good(field.set_text("edited"));good(table->commit_edit());
    require(take(source->snapshot())->revision==2&&take(source->snapshot())->rows[0].cells[0].text=="edited"&&
            take(source->snapshot())->rows[1].cells[1].text=="1/1","Editing a merge changed hidden cells or the wrong anchor");
    try_nested=true;
    require(take(table->apply_merge({.revision=2,.action=TableMergeAction::split,.range={.row=1,.column=1}}))==3&&
            changes==1&&nested_rejected&&take(table->frame())->merges.empty(),"Table did not apply and publish a split safely");
    try_nested=false;
    require(take(table->apply_merge({.revision=3,.action=TableMergeAction::split,.range={.row=1,.column=1}}))==3&&changes==1,
            "A no-op split published a change");
    require(has_text(take(tree->drawing_snapshot({200,100})),"1/1"),"A split did not restore hidden cell drawing");
    std::vector<TableColumn> dense_columns;std::vector<TableRow> dense_rows;
    for(int c=0;c<33;++c)dense_columns.push_back({.key="c"+std::to_string(c),.width=32});
    for(int r=0;r<33;++r){TableRow row;row.key="r"+std::to_string(r);row.height=18;row.cells.resize(33);dense_rows.push_back(std::move(row));}
    auto dense_source=std::make_shared<TableMemory>(dense_columns,dense_rows);TableMemoryMergeEditor dense_editor(dense_source);
    require(take(dense_editor.apply({.revision=0,.action=TableMergeAction::merge,
                                    .range={.row=0,.column=0,.row_count=2,.column_count=2}}))==1,"Dense merge setup failed");
    native::TableInput dense_input;dense_input.controller=std::make_shared<TableController>(dense_source);
    dense_input.appearance.width=1056;dense_input.appearance.height=618;
    auto rejected=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto dense=native::NativeTable::create(builder,{},"dense",dense_input);
        return dense?ii::core::Result<void>{}:std::unexpected(dense.error());
    });
    require(!rejected&&table->root().snapshot()&&has_text(take(tree->drawing_snapshot({200,100})),"1/1"),
            "A merged table beyond the drawing budget replaced the committed view");
    require(!table->apply_merge({.revision=2,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=2,.column_count=2}})&&
            take(source->snapshot())->revision==3,"A stale merge modified the table");
    auto unavailable=input;unavailable.merge_editor.reset();good(table->set_options(unavailable));
    require(!table->apply_merge({.revision=3,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=2,.column_count=2}}),
            "A table without a merge editor accepted a merge");
    good(table->set_options(input));retire=true;
    require(take(table->apply_merge({.revision=3,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=2,.column_count=2}}))==4&&
            changes==2&&!table->set_options(input),"A merge callback could not retire its view safely");
    tree->close();

    std::vector<TableColumn> huge_columns;std::vector<TableRow> huge_rows;
    for(int c=0;c<3;++c)huge_columns.push_back({.key="c"+std::to_string(c),.width=8192});
    for(int r=0;r<3;++r){TableRow row;row.key="r"+std::to_string(r);row.height=8192;row.cells.resize(3);row.cells[0].text="large";huge_rows.push_back(std::move(row));}
    auto huge_source=std::make_shared<TableMemory>(huge_columns,huge_rows);TableMemoryMergeEditor huge_editor(huge_source);
    require(take(huge_editor.apply({.revision=0,.action=TableMergeAction::merge,
                                  .range={.row=0,.column=0,.row_count=3,.column_count=3}}))==1,"Large merge setup failed");
    native::TableInput huge_input;huge_input.controller=std::make_shared<TableController>(huge_source);
    huge_input.appearance.width=160;huge_input.appearance.height=72;
    auto huge_tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto huge_host=take(huge_tree->content_host({},"huge-host",{200,100}));std::optional<native::NativeTable> huge_table;
    good(huge_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto view=native::NativeTable::create(builder,*column,"huge",huge_input);if(!view)return std::unexpected(view.error());huge_table=*view;return {};
    }));
    (void)take(huge_tree->drawing_snapshot({200,100}));good(huge_table->scroll().set_scroll_position(9000,9000));
    require(take(huge_table->frame())->merges.size()==1&&valid_canvas(take(huge_tree->drawing_snapshot({200,100}))),
            "A very large partially visible merge produced invalid drawing geometry");
    good(huge_host.clear_content());huge_tree->close();
}
