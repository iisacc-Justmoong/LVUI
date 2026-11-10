#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;
    auto source=std::make_shared<lvui::TableMemory>(std::vector<lvui::TableColumn>{{.key="a",.title="A",.width=80},{.key="b",.title="B",.width=80}},
        std::vector<lvui::TableRow>{{.key="r0",.cells={{.text="A"},{.text="B"}}},{.key="r1",.cells={{.text="C"},{.text="D"}}}});
    auto editor=std::make_shared<lvui::TableMemoryStructureEditor>(source);
    lvui::native::TableInput input;input.controller=std::make_shared<lvui::TableController>(source);
    input.structure_editor=editor;input.structure_controls_visible=true;input.appearance.width=220;input.appearance.height=160;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::optional<lvui::native::NativeTable> table;
    int changes=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto created=lvui::native::NativeTable::create(builder,*stack,"table",input,{.structure_changed=[&](const auto&){++changes;}});
        if(!created)return std::unexpected(created.error());table=*created;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));
    const auto find=[&](std::string_view suffix){const auto nodes=take(tree->snapshots());
        const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
        if(item==nodes.end())throw std::runtime_error("Missing table accessibility host");return take(tree->find(item->id));};
    auto rows=find(".row-actions"),columns=find(".column-actions");
    auto row0=take(rows.keyed_content_item("r0","action")),row1=take(rows.keyed_content_item("r1","action"));
    auto col0=take(columns.keyed_content_item("a","action"));
    const auto initial=take(row1.snapshot());
    require(initial.action_role==ActionRole::button&&take(row1.action_options()).focus_ring&&initial.accessibility_name=="Add row after row 2"&&
            take(col0.snapshot()).accessibility_name=="Add column after column 1", "Visible structure actions lack accessible roles or names");
    good(row0.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    auto value=take(source->snapshot());require(value->revision==1&&value->rows.size()==3&&value->rows[1].key=="lvui-row-1"&&changes==1,
        "Keyboard activation did not insert after the targeted row");
    require(take(row1.snapshot()).id==initial.id&&take(row1.snapshot()).accessibility_name=="Add row after row 3",
        "An existing row action lost identity or its position after insertion");
    good(tree->input({.kind=WidgetInputKind::activate,.target=take(col0.snapshot()).id}));value=take(source->snapshot());
    require(value->revision==2&&value->columns.size()==3&&value->columns[1].key=="lvui-column-1"&&changes==2,
        "Accessibility activation did not insert after the targeted column");
    input.add_row_controls_visible=false;good(table->set_options(input));
    require(!take(row1.snapshot()).visible&&take(col0.snapshot()).visible,"Hiding row controls left their accessibility actions exposed");
    input.add_row_controls_visible=true;good(table->set_options(input));
    require(take(row1.snapshot()).visible&&take(row1.snapshot()).id==initial.id,"Restoring row controls discarded a retained action");
    const auto before_scroll=value->revision;good(table->scroll().set_scroll_position(40,30));
    require(take(source->snapshot())->revision==before_scroll,"Scrolling accessible structure actions changed the document");
    (void)take(editor->apply({.revision=2,.operations={lvui::TableResizeRows{{{"r1",30}}}}}));
    good(row1.activate());require(take(source->snapshot())->revision==3&&changes==2&&take(table->failure()).has_value(),
        "A stale accessibility action committed against newer table data");
    input.source_revision=3;input.appearance.enabled=false;good(table->set_options(input));
    good(row1.activate());require(take(source->snapshot())->revision==3,"A disabled accessible action changed the table");
    good(host.clear_content());require(!row1.snapshot(),"A retired table retained an accessible structure action");tree->close();

    std::vector<lvui::TableColumn> many_columns;std::vector<lvui::TableRow> many_rows;
    for(int i=0;i<8;++i)many_columns.push_back({.key="c"+std::to_string(i),.width=80});
    for(int i=0;i<30;++i)many_rows.push_back({.key="row"+std::to_string(i),.cells=std::vector<lvui::TableCell>(8)});
    auto large_source=std::make_shared<lvui::TableMemory>(std::move(many_columns),std::move(many_rows));
    lvui::native::TableInput large;large.controller=std::make_shared<lvui::TableController>(large_source);
    large.structure_editor=std::make_shared<lvui::TableMemoryStructureEditor>(large_source);large.structure_controls_visible=true;
    large.appearance.width=220;large.appearance.height=160;
    auto scrolling=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto scrolling_host=take(scrolling->content_host({},"host",{500,300}));std::optional<lvui::native::NativeTable> scrolling_table;
    good(scrolling_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeTable::create(builder,{},"large",large);if(!created)return std::unexpected(created.error());scrolling_table=*created;return {};
    }));
    (void)take(scrolling->drawing_snapshot({500,300}));
    const auto nodes=take(scrolling->snapshots());const auto node=std::ranges::find_if(nodes,[](const auto& item){return item.id.ends_with(".row-actions");});
    require(node!=nodes.end(),"Scrolled table did not expose row actions");auto scrolling_rows=take(scrolling->find(node->id));
    auto stable=take(scrolling_rows.keyed_content_item("row1","action")),retiring=take(scrolling_rows.keyed_content_item("row0","action"));
    const auto stable_id=take(stable.snapshot()).id;good(scrolling_table->scroll().set_scroll_position(0,10));
    require(take(stable.snapshot()).id==stable_id&&take(large_source->snapshot())->revision==0,
        "Scrolling changed a retained accessibility action or the document revision");
    good(scrolling_table->scroll().set_scroll_position(170,240));
    require(!retiring.snapshot()&&take(large_source->snapshot())->revision==0,"An offscreen accessibility action did not retire");
    auto visible=take(scrolling_rows.keyed_content_item("row10","action"));
    require(take(visible.snapshot()).accessibility_name=="Add row after row 11","Scrolled action reported the wrong row");
    good(scrolling->input({.kind=WidgetInputKind::activate,.target=take(visible.snapshot()).id}));
    require(take(large_source->snapshot())->rows[11].key=="lvui-row-1","A scrolled accessibility action inserted at the wrong row");
    good(scrolling_host.clear_content());scrolling->close();

    const auto nested=[&](bool controls){
        auto nested_tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
        auto root=take(nested_tree->content_host({},"root",{500,300}));auto options=large;std::optional<lvui::native::NativeTable> nested_table;
        options.appearance.enabled=true;options.structure_controls_visible=controls;options.add_row_controls_visible=true;options.add_column_controls_visible=true;
        std::function<ii::core::Result<void>(WidgetBuilder&,int)> build=[&](WidgetBuilder& builder,int remaining)->ii::core::Result<void>{
            if(!remaining){auto result=lvui::native::NativeTable::create(builder,{},"deep-table",options);if(!result)return std::unexpected(result.error());nested_table=*result;return {};}
            auto child=builder.content_host({},"nest-"+std::to_string(remaining),{220,160});if(!child)return std::unexpected(child.error());
            return child->set_content([&,remaining](WidgetBuilder& inner){return build(inner,remaining-1);});
        };
        const auto result=root.set_content([&](WidgetBuilder& builder){return build(builder,6);});
        if(result&&controls){
            auto nodes=take(nested_tree->snapshots());
            auto action=std::ranges::find_if(nodes,[](const auto& value){return value.accessibility_name=="Add row after row 2";});
            require(action!=nodes.end()&&action->visible&&action->action_role==ActionRole::button,
                "The compact nested table lost its accessibility action");
            auto stable=take(nested_tree->find(action->id));const auto stable_id=take(stable.snapshot()).id;good(stable.focus());
            good(nested_table->scroll().set_scroll_position(0,1));
            require(take(stable.snapshot()).id==stable_id&&take(stable.snapshot()).focused,"A small scroll replaced or defocused a compact accessibility action");
            good(nested_table->scroll().set_scroll_position(0,240));require(!stable.snapshot(),"A compact offscreen action remained live");
            nodes=take(nested_tree->snapshots());action=std::ranges::find_if(nodes,[](const auto& value){return value.accessibility_name=="Add row after row 11";});
            require(action!=nodes.end()&&action->visible,"A compact scrolled action reported the wrong row");
            const auto revision=take(large_source->snapshot())->revision;
            good(nested_tree->input({.kind=WidgetInputKind::activate,.target=action->id}));
            require(take(large_source->snapshot())->revision==revision+1,"The compact nested accessibility action did not insert a row");
        }
        nested_tree->close();return bool(result);
    };
    require(nested(false),"The table depth baseline was not valid");
    require(nested(true),"Accessible structure actions prevented an otherwise valid nested table from mounting");

    const auto capacity=[&](std::size_t filler,bool controls)->std::optional<std::size_t>{
        auto domain=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
        auto root=take(domain->content_host({},"root",{500,300}));auto options=input;std::optional<lvui::native::NativeTable> capacity_table;
        options.appearance.enabled=true;options.structure_controls_visible=controls;options.add_row_controls_visible=true;options.add_column_controls_visible=true;
        const auto mounted=root.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            auto stack=builder.absolute_stack({},"stack",{500,300});if(!stack)return std::unexpected(stack.error());
            for(std::size_t i=0;i<filler;++i){auto spacer=builder.spacer(*stack,"filler-"+std::to_string(i));if(!spacer)return std::unexpected(spacer.error());}
            auto created=lvui::native::NativeTable::create(builder,*stack,"capacity-table",options);if(!created)return std::unexpected(created.error());capacity_table=*created;return {};
        });
        if(!mounted){domain->close();return std::nullopt;}
        const auto nodes=take(domain->snapshots());
        if(controls)require(std::ranges::any_of(nodes,[](const auto& item){return item.accessibility_name=="Add column after column 1";}),
            "The capacity fallback did not expose column actions");
        const auto result=nodes.size();
        if(controls&&filler){
            const auto action=std::ranges::find_if(nodes,[](const auto& item){return item.accessibility_name=="Add row after row 1";});
            require(action!=nodes.end(),"The capacity fallback lost row actions");const auto revision=take(source->snapshot())->revision;
            good(domain->input({.kind=WidgetInputKind::activate,.target=action->id}));
            require(take(source->snapshot())->revision==revision+1&&!take(capacity_table->failure()).has_value(),
                "An insertion at the widget limit failed to refresh the visible table");
        }
        domain->close();return result;
    };
    const auto base=capacity(0,false),normal=capacity(0,true);
    const auto capacity_data=take(source->snapshot());const auto slack=2+2*capacity_data->rows.size()+capacity_data->columns.size();
    require(base&&normal&&*normal>*base+slack&&*base+slack<4096,"Table accessibility capacity setup was invalid");
    const auto bounded=capacity(4096-*base-slack,true);
    require(bounded&&*bounded<=4096,"Accessibility actions prevented an otherwise valid table near the widget limit");
}
