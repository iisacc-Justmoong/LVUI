#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
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
const ii::gui::WidgetSnapshot& named(const std::vector<ii::gui::WidgetSnapshot>& nodes,std::string_view value){
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.native_class=="iiGUI.VirtualAccessibility"&&node.accessibility_name==value;});
    if(found==nodes.end())throw std::runtime_error("Missing accessible cell: "+std::string(value));return *found;
}
void basic(){
    using namespace ii::gui;using namespace lvui;
    std::vector<TableColumn> columns{{.key="amount",.title="Amount",.width=80},{.key="locked",.title="Locked",.width=80}};
    std::vector<TableRow> rows;for(int r=0;r<20;++r)rows.push_back({.key="r"+std::to_string(r),
        .cells={{.text="value "+std::to_string(r)},{.text="locked",.enabled=r!=0}}});
    auto source=std::make_shared<TableMemory>(columns,rows);auto controller=std::make_shared<TableController>(source);
    native::TableInput input;input.controller=controller;input.editor=source;input.editable=true;
    input.appearance.width=160;input.appearance.height=96;input.appearance.accessibility_name="Amounts";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{220,150}));std::optional<native::NativeTable> table;int changes=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=native::NativeTable::create(builder,{},"table",input,{.selection_changed=[&](const auto&){++changes;}});
        if(!created)return std::unexpected(created.error());table=*created;return {};
    }));
    (void)take(tree->drawing_snapshot({220,150}));auto nodes=take(tree->accessibility_snapshots());
    const auto first=named(nodes,"Amount, row 1"), second=named(nodes,"Amount, row 2"), locked=named(nodes,"Locked, row 1");
    const auto body=take(table->body().snapshot());
    const auto semantic_table=take(table->root().snapshot());
    require(semantic_table.accessibility_semantics&&semantic_table.accessibility_semantics->role==AccessibilityRole::table&&
            first.parent_id==body.id&&first.text=="value 0"&&!locked.enabled,"Table cell semantics or value was lost");
    good(tree->input({.kind=WidgetInputKind::focus,.target=first.id}));
    require(take(controller->selection())->current==TablePosition{"r0","amount"}&&take(source->snapshot())->revision==0&&changes==1,
            "Accessible focus did not select the cell without editing the document");
    require(take(tree->accessibility_snapshot(first.id)).selected,"Cell selection was not republished");
    good(tree->input({.kind=WidgetInputKind::activate,.target=locked.id}));
    require(take(controller->selection())->current==TablePosition{"r0","amount"}&&take(source->snapshot())->revision==0,
            "A disabled accessible cell changed selection or data");
    good(tree->input({.kind=WidgetInputKind::activate,.target=first.id}));
    require(bool(table->editor()),"Accessible activation did not open the cell editor");
    good(take(table->editor()).set_text("edited"));good(table->commit_edit());
    require(take(source->snapshot())->revision==1&&take(tree->accessibility_snapshot(first.id)).text=="edited",
            "Accessible editing did not commit to the cell or refresh its value");
    good(table->scroll().set_scroll_position(0,24));
    require(!tree->accessibility_snapshot(first.id)&&take(tree->accessibility_snapshot(second.id)).text=="value 1",
            "Scrolling did not retire hidden cells or retain visible cell identities");
    const auto before=take(controller->selection());
    (void)take(source->edit({.revision=1,.cells={{"r1","amount","newer"}}}));
    auto stale=tree->input({.kind=WidgetInputKind::activate,.target=second.id});
    require(!stale&&take(source->snapshot())->revision==2&&take(source->snapshot())->rows[1].cells[0].text=="newer",
            "A stale accessibility action overwrote newer data");
    require(!table->editor()&&take(controller->selection())==before,"A stale cell action changed selection or opened an editor");
    good(host.clear_content());require(!tree->accessibility_snapshot(second.id),"Retired table exposed a virtual cell");tree->close();
}
void merged(){
    using namespace ii::gui;using namespace lvui;
    std::vector<TableColumn> columns;std::vector<TableRow> rows;
    for(int c=0;c<5;++c)columns.push_back({.key="c"+std::to_string(c),.title="C"+std::to_string(c),.width=80});
    for(int r=0;r<5;++r){TableRow row;row.key="r"+std::to_string(r);for(int c=0;c<5;++c)row.cells.push_back({.text=r==0&&c==0?"anchor":"covered"});rows.push_back(std::move(row));}
    auto source=std::make_shared<TableMemory>(columns,rows);TableMemoryMergeEditor editor(source);
    (void)take(editor.apply({.revision=0,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=3,.column_count=3}}));
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.appearance.width=160;input.appearance.height=72;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto host=take(tree->content_host({},"host",{200,100}));
    std::optional<native::NativeTable> table;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto created=native::NativeTable::create(builder,{},"merged",input);if(!created)return std::unexpected(created.error());table=*created;return {};}));
    (void)take(tree->drawing_snapshot({200,100}));good(table->scroll().set_scroll_position(90,30));
    const auto nodes=take(tree->accessibility_snapshots());const auto& anchor=named(nodes,"C0, row 1");
    require(anchor.visible&&anchor.text=="anchor"&&std::ranges::count_if(nodes,[](const auto& item){return item.native_class=="iiGUI.VirtualAccessibility"&&item.text=="anchor";})==1,
            "A partially visible merged cell was not represented once by its anchor");
    good(tree->input({.kind=WidgetInputKind::focus,.target=anchor.id}));
    require(take(input.controller->selection())->current==TablePosition{"r0","c0"},"A merged cell accessibility action selected a covered cell");
    good(host.clear_content());tree->close();
}
void long_text(){
    using namespace ii::gui;using namespace lvui;
    const std::string row_key(1024,'r'),column_key(1024,'c');
    auto source=std::make_shared<TableMemory>(std::vector<TableColumn>{{.key=column_key,.title=std::string(255,'x')+"한글",.width=80}},
        std::vector<TableRow>{{.key=row_key,.cells={{.text=std::string(1023,'v')+"한글"}}}});
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.appearance.width=100;input.appearance.height=60;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto host=take(tree->content_host({},"host",{120,80}));
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto created=native::NativeTable::create(builder,{},"table",input);return created?ii::core::Result<void>{}:std::unexpected(created.error());}));
    (void)take(tree->drawing_snapshot({120,80}));const auto nodes=take(tree->accessibility_snapshots());
    const auto cell=named(nodes,std::string(255,'x')+", row 1");
    require(cell.text==std::string(1023,'v'),"A UTF-8 accessibility excerpt ended inside a character");
    good(tree->input({.kind=WidgetInputKind::focus,.target=cell.id}));
    require(take(input.controller->selection())->current==TablePosition{row_key,column_key},"Long cell keys lost their identity");
    good(host.clear_content());tree->close();
}
void capacity(){
    using namespace ii::gui;using namespace lvui;
    std::vector<TableColumn> columns;std::vector<TableRow> rows;
    for(int c=0;c<32;++c)columns.push_back({.key="c"+std::to_string(c),.width=32});
    for(int r=0;r<28;++r){TableRow row;row.key="r"+std::to_string(r);row.height=18;row.cells.resize(32);rows.push_back(std::move(row));}
    auto source=std::make_shared<TableMemory>(std::move(columns),std::move(rows));
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.appearance.width=1024;input.appearance.height=528;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto host=take(tree->content_host({},"host",{1100,600}));
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{1100,600});if(!root)return std::unexpected(root.error());
        for(int i=0;i<3300;++i){auto filler=builder.spacer(*root,"spacer-"+std::to_string(i));if(!filler)return std::unexpected(filler.error());}
        auto created=native::NativeTable::create(builder,*root,"table",input);return created?ii::core::Result<void>{}:std::unexpected(created.error());
    }));
    (void)take(tree->drawing_snapshot({1100,600}));
    const auto actual=take(tree->snapshots()).size(),accessible=take(tree->accessibility_snapshots()).size();
    require(actual<4096&&accessible>4096,"Virtual table cells consumed the retained widget budget");
    good(host.clear_content());tree->close();
}
void reentrant_retirement(){
    using namespace ii::gui;using namespace lvui;
    auto source=std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="c",.title="C",.width=80}},
        std::vector<TableRow>{{.key="r",.cells={{.text="value"}}}});
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.appearance.width=100;input.appearance.height=60;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto host=take(tree->content_host({},"host",{120,80}));
    std::optional<native::NativeTable> table;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=native::NativeTable::create(builder,{},"table",input,{.selection_changed=[&](const auto&){good(host.clear_content());}});
        if(!created)return std::unexpected(created.error());table=*created;return {};
    }));
    (void)take(tree->drawing_snapshot({120,80}));const auto nodes=take(tree->accessibility_snapshots());const auto cell=named(nodes,"C, row 1");
    good(tree->input({.kind=WidgetInputKind::focus,.target=cell.id}));
    require(!table->root().snapshot()&&!tree->accessibility_snapshot(cell.id)&&take(source->snapshot())->revision==0,
            "A selection callback retiring the table left a live accessibility cell or changed the document");
    tree->close();
}
void localized_text(){
    using namespace ii::gui;using namespace lvui;
    auto source=std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="a",.title="값 {row}",.width=80},{.key="b",.width=80}},
        std::vector<TableRow>{{.key="r0",.cells={{.text="one"},{.text="two"}}},{.key="r1",.cells={{.text="three"},{.text="four"}}}});
    TableMemoryMergeEditor merger(source);
    (void)take(merger.apply({.revision=0,.action=TableMergeAction::merge,.range={.row=0,.column=0,.row_count=2,.column_count=1}}));
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.editor=source;input.editable=true;
    input.structure_editor=std::make_shared<TableMemoryStructureEditor>(source);input.structure_controls_visible=true;
    input.appearance.width=220;input.appearance.height=120;
    input.text.unnamed_column="{column}열";input.text.cell_name="{row}행의 {column}";
    input.text.merged_description="{columns}열과 {rows}행 병합";
    input.text.add_row_after="{row}행 다음에 삽입";input.text.add_column_after="{column}열 다음에 삽입";
    input.text.add_first_row="첫 행 추가";input.text.add_first_column="첫 열 추가";
    input.text.edit_cell="셀 편집";input.text.structure_menu="표 구조";
    input.text.delete_row="행 삭제";input.text.delete_column="열 삭제";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{260,160}));std::optional<native::NativeTable> table;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto value=native::NativeTable::create(builder,{},"table",input);
        if(!value)return std::unexpected(value.error());table=*value;return {};}));
    (void)take(tree->drawing_snapshot({260,160}));auto nodes=take(tree->accessibility_snapshots());
    const auto anchor=named(nodes,"1행의 값 {row}");const auto fallback=named(nodes,"1행의 2열");
    require(anchor.accessibility_description=="1열과 2행 병합"&&fallback.text=="two","Localized cell names or merge description were wrong");
    const auto has=[&](std::string_view text){auto current=take(tree->snapshots());return std::ranges::any_of(current,[&](const auto& node){return node.accessibility_name==text;});};
    require(has("1행 다음에 삽입")&&has("1열 다음에 삽입")&&has("행 삭제")&&has("열 삭제"),"Localized table controls were absent");
    good(tree->input({.kind=WidgetInputKind::activate,.target=anchor.id}));require(bool(table->editor()),"Localized cell did not open its editor");
    good(take(table->editor()).set_text("draft"));require(has("셀 편집"),"Localized editor name was absent");
    good(take(table->editor()).set_selection(0,2));good(tree->input({.kind=WidgetInputKind::composition,.text="하"}));
    const auto composing=take(take(table->editor()).snapshot());require(composing.marked.has_value(),"The test did not start input composition");
    const auto selected=take(input.controller->selection());const auto revision=take(source->snapshot())->revision;
    input.text.cell_name="{column} ({row})";input.text.edit_cell="수정";input.text.add_row_after="행 {row} 뒤에 추가";
    input.text.delete_row="행 지우기";good(table->set_options(input));
    const auto updated=take(tree->accessibility_snapshot(anchor.id));
    const auto still_composing=take(take(table->editor()).snapshot());
    require(updated.accessibility_name=="값 {row} (1)"&&still_composing.text==composing.text&&still_composing.marked==composing.marked&&
            still_composing.selection==composing.selection&&has("수정")&&has("행 1 뒤에 추가")&&has("행 지우기"),
        "Updating locale lost a cell identity, current editor draft, or control label");
    require(take(source->snapshot())->revision==revision&&take(input.controller->selection())==selected,"Changing locale changed table data or selection");
    for(const auto& invalid:{std::string("{row}"),std::string("{column} {row} {row}"),std::string("{column} {bad}"),std::string("{column} {row"),std::string(513,'x'),std::string("{column} {row}\xff")}){
        auto rejected=input;rejected.text.cell_name=invalid;require(!table->set_options(rejected),"Invalid localization was accepted");
        require(take(tree->accessibility_snapshot(anchor.id)).accessibility_name=="값 {row} (1)"&&has("수정"),"Invalid localization changed the live table");
    }
    input.text.cell_name="{{{row}}} {column}";good(table->set_options(input));
    require(take(tree->accessibility_snapshot(anchor.id)).accessibility_name=="{1} 값 {row}","Escaped braces or untrusted title were formatted incorrectly");
    good(host.clear_content());
    auto empty=std::make_shared<TableMemory>();input.controller=std::make_shared<TableController>(empty);input.editor.reset();input.editable=false;
    input.structure_editor=std::make_shared<TableMemoryStructureEditor>(empty);table.reset();
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto value=native::NativeTable::create(builder,{},"empty",input);
        if(!value)return std::unexpected(value.error());table=*value;return {};}));
    require(has("첫 행 추가")&&has("첫 열 추가"),"Localized initial row and column actions were absent");
    input.text.add_first_row="행 시작";input.text.add_first_column="열 시작";good(table->set_options(input));
    require(has("행 시작")&&has("열 시작")&&take(empty->snapshot())->revision==0,"Updating initial labels modified the empty table");
    good(host.clear_content());tree->close();
}
}
int main(){basic();merged();long_text();capacity();reentrant_retirement();localized_text();}
