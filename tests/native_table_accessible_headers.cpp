#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include "table_header_test_binding.h"
#include "Widgets/AccessibilityGrouping.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
const ii::gui::WidgetSnapshot& column(const std::vector<ii::gui::WidgetSnapshot>& nodes,std::string_view name){
    auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.native_class=="iiGUI.VirtualAccessibility"&&
        node.accessibility_semantics&&node.accessibility_semantics->role==ii::gui::AccessibilityRole::column_header&&node.accessibility_name==name;});
    if(found==nodes.end())throw std::runtime_error("Missing accessible column: "+std::string(name));return *found;
}
void belongs(const std::vector<ii::gui::WidgetSnapshot>& nodes,const ii::gui::WidgetSnapshot& child,std::string_view parent){
    const auto groups=ii::gui::detail::accessibility_group_parents(nodes);const auto index=std::size_t(&child-nodes.data());
    require(groups[index]&&nodes[*groups[index]].id==parent,"A column header was not grouped with its owner");
}
void table(){
    using namespace ii::gui;using namespace lvui;
    auto source=std::make_shared<TableMemory>(
        std::vector<TableColumn>{{.key="a",.title="Amount",.width=80},{.key="b",.title="Locked",.width=80,.enabled=false},{.key="c",.width=80}},
        std::vector<TableRow>{{.key="r0",.cells={{.text="2"},{},{}}},{.key="r1",.cells={{.text="1"},{},{}}}});
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.order_editor=std::make_shared<TableMemoryOrderEditor>(source);
    input.sorting_enabled=true;input.appearance.width=160;input.appearance.height=96;input.appearance.accessibility_name="Amounts";
    input.text.unnamed_column="{column}열";input.text.sortable_column="정렬 가능";
    input.text.sorted_ascending="오름차순";input.text.sorted_descending="내림차순";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{220,140}));std::optional<native::NativeTable> table;int sorts=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeTable::create(builder,{},"table",input,{.sorted=[&](std::size_t,TableSortOrder){++sorts;}});
        if(!value)return std::unexpected(value.error());table=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({220,140}));auto nodes=take(tree->accessibility_snapshots());
    const auto amount=column(nodes,"Amount"),locked=column(nodes,"Locked");
    require(amount.accessibility_actionable&&amount.accessibility_description=="정렬 가능"&&!locked.enabled,
            "Table header action, description, or disabled state was lost");
    belongs(nodes,column(nodes,"Amount"),take(table->root().snapshot()).id);
    auto cell=std::ranges::find_if(nodes,[](const auto& node){return node.accessibility_semantics&&node.accessibility_semantics->role==AccessibilityRole::cell;});
    require(cell!=nodes.end(),"The table lost accessible cells");belongs(nodes,*cell,take(table->root().snapshot()).id);
    good(tree->input({.kind=WidgetInputKind::focus,.target=amount.id}));
    require(take(tree->accessibility_snapshot(amount.id)).focused&&take(source->snapshot())->revision==0,
            "Header focus changed data or failed to publish its target");
    good(tree->input({.kind=WidgetInputKind::activate,.target=locked.id}));require(sorts==0,"A disabled header sorted the table");
    good(tree->input({.kind=WidgetInputKind::activate,.target=amount.id}));
    require(sorts==1&&take(source->snapshot())->rows.front().key=="r1"&&
            take(tree->accessibility_snapshot(amount.id)).accessibility_description=="오름차순",
            "Header activation did not sort ascending or announce its state");
    input.text.sorted_ascending="오름차순 정렬됨";good(table->set_options(input));
    require(sorts==1&&take(tree->accessibility_snapshot(amount.id)).accessibility_description=="오름차순 정렬됨",
            "Changing the locale lost the current sort state or invoked a sort");
    good(tree->input({.kind=WidgetInputKind::activate,.target=amount.id}));
    require(sorts==2&&take(source->snapshot())->rows.front().key=="r0"&&
            take(tree->accessibility_snapshot(amount.id)).accessibility_description=="내림차순",
            "The retained header did not toggle descending");
    const auto revision=take(source->snapshot())->revision;
    (void)take(source->edit({.revision=revision,.cells={{"r0","a","9"}}}));
    auto stale=tree->input({.kind=WidgetInputKind::activate,.target=amount.id});
    require(!stale&&take(source->snapshot())->revision==revision+1&&sorts==2,"A stale header action sorted newer data");
    input.source_revision=std::int64_t(revision+1);input.sorting_enabled=false;good(table->set_options(input));
    const auto passive=take(tree->accessibility_snapshot(amount.id));require(!passive.accessibility_actionable&&passive.enabled,"A passive header was exposed as an active button");
    good(tree->input({.kind=WidgetInputKind::activate,.target=amount.id}));require(sorts==2,"A passive header sorted the table");
    good(table->scroll().set_scroll_position(80,0));nodes=take(tree->accessibility_snapshots());
    const auto unnamed=column(nodes,"3열");belongs(nodes,column(nodes,"3열"),take(table->root().snapshot()).id);
    require(!tree->accessibility_snapshot(amount.id)&&unnamed.visible,"Scrolling did not retire and expose the correct headers");
    good(host.clear_content());require(!tree->accessibility_snapshot(unnamed.id),"A retired table retained a header");tree->close();
}
void standalone(){
    using namespace ii::gui;using namespace lvui;
    native::TableHeaderInput input;input.configuration.width=160;input.configuration.interactive=true;
    input.configuration.columns={{.key="a",.title="First",.width=80},{.key="b",.title="Disabled",.width=80,.enabled=false},{.key="c",.width=80}};
    input.text.header_group="열 머리글";input.text.unnamed_column="{column}열";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,80}));std::optional<TableHeaderTestView> header;int clicks=0;bool retire=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.absolute_stack({},"header-layout",{160,25});if(!stack)return std::unexpected(stack.error());
        auto value=TableHeaderTestView::create(builder,*stack,"header",input,{.column_clicked=[&](std::size_t index,const TableColumn&){
            require(index==0,"The wrong standalone column was activated");++clicks;if(retire)good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());header=*value;return header->root().set_frame({0,0,160,25});
    }));
    (void)take(tree->drawing_snapshot({200,80}));auto nodes=take(tree->accessibility_snapshots());
    auto first=column(nodes,"First");belongs(nodes,column(nodes,"First"),take(header->root().snapshot()).id);
    require(take(header->root().snapshot()).accessibility_semantics->name=="열 머리글"&&
            !take(header->root().snapshot()).accessibility_actionable&&!column(nodes,"Disabled").enabled,
            "Standalone header group or disabled column was wrong");
    good(tree->input({.kind=WidgetInputKind::focus,.target=first.id}));
    require(take(tree->accessibility_snapshot(first.id)).focused,"Standalone accessible focus was not published");
    good(tree->input({.kind=WidgetInputKind::activate,.target=first.id}));require(clicks==1,"Standalone accessible activation did not invoke its callback");
    input.configuration.columns[0].title="Renamed";good(header->set_options(input));
    require(take(tree->accessibility_snapshot(first.id)).accessibility_name=="Renamed","A renamed column lost its identity");
    auto invalid=input;invalid.configuration.columns[1].key="a";
    require(!header->set_options(invalid)&&take(tree->accessibility_snapshot(first.id)).accessibility_name=="Renamed",
            "An invalid header replaced its published accessibility children");
    invalid=input;invalid.text.header_group.clear();
    require(!header->set_options(invalid)&&take(header->root().snapshot()).accessibility_semantics->name=="열 머리글",
            "Invalid localized header text replaced the existing group");
    input.offset=80;input.configuration.interactive=false;good(header->set_options(input));nodes=take(tree->accessibility_snapshots());
    auto third=column(nodes,"3열");require(!third.accessibility_actionable,"Passive standalone column remained actionable");
    if(tree->accessibility_snapshot(first.id)){const auto viewport=take(header->viewport());
        throw std::runtime_error("Scrolled-out standalone column remained accessible: x="+std::to_string(viewport.x)+" width="+std::to_string(viewport.width));}
    good(tree->input({.kind=WidgetInputKind::activate,.target=third.id}));require(clicks==1,"A passive standalone header invoked a callback");
    input.offset=0;input.configuration.interactive=true;good(header->set_options(input));nodes=take(tree->accessibility_snapshots());
    first=column(nodes,"Renamed");retire=true;good(tree->input({.kind=WidgetInputKind::activate,.target=first.id}));
    require(clicks==2&&!tree->accessibility_snapshot(first.id),"A reentrant callback left a live standalone header");tree->close();
}
void table_retirement(){
    using namespace ii::gui;using namespace lvui;
    auto source=std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="a",.title="A",.width=80}},
        std::vector<TableRow>{{.key="r0",.cells={{.text="2"}}},{.key="r1",.cells={{.text="1"}}}});
    native::TableInput input;input.controller=std::make_shared<TableController>(source);input.order_editor=std::make_shared<TableMemoryOrderEditor>(source);
    input.sorting_enabled=true;input.appearance.width=100;input.appearance.height=70;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto host=take(tree->content_host({},"host",{120,100}));
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto table=native::NativeTable::create(builder,{},"table",input,
        {.sorted=[&](std::size_t,TableSortOrder){good(host.clear_content());}});return table?ii::core::Result<void>{}:std::unexpected(table.error());}));
    (void)take(tree->drawing_snapshot({120,100}));auto nodes=take(tree->accessibility_snapshots());const auto first=column(nodes,"A");
    good(tree->input({.kind=WidgetInputKind::activate,.target=first.id}));
    require(take(source->snapshot())->revision==1&&!tree->accessibility_snapshot(first.id),"A sorting callback retained a retired table header");tree->close();
}
}
int main(){table();standalone();table_retirement();}
