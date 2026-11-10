#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
class InitialFactory final : public lvui::TableStructureFactory {
public:
    bool invalid=false,throws=false;
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot& value,std::size_t index) const override {
        if(throws)throw std::runtime_error("Domain factory failed");
        if(index!=0)return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Wrong first-row index"});
        return lvui::TableRow{.key=invalid?"":"first-row",.cells=std::vector<lvui::TableCell>(value.columns.size(),{.text="row"})};
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot& value,std::size_t index) const override {
        if(index!=0)return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Wrong first-column index"});
        return lvui::TableColumnInsertion{.column={.key=invalid?"":"first-column"},
            .cells=std::vector<lvui::TableCell>(value.rows.size(),{.text="column"})};
    }
};
struct Fixture final {
    using Widget=ii::gui::Widget;
    std::shared_ptr<lvui::TableMemory> source;
    std::shared_ptr<lvui::TableMemoryStructureEditor> editor;
    std::shared_ptr<InitialFactory> factory=std::make_shared<InitialFactory>();
    lvui::native::TableInput input;
    std::shared_ptr<ii::gui::detail::WidgetState> tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    Widget host;
    std::optional<lvui::native::NativeTable> table;
    int changes=0,selections=0;std::function<void()> on_change;
    Fixture(std::vector<lvui::TableColumn> columns={},std::vector<lvui::TableRow> rows={}){
        using namespace ii::gui;
        source=std::make_shared<lvui::TableMemory>(std::move(columns),std::move(rows));
        editor=std::make_shared<lvui::TableMemoryStructureEditor>(source);
        input.controller=std::make_shared<lvui::TableController>(source);input.structure_editor=editor;input.structure_factory=factory;
        input.structure_controls_visible=true;input.appearance.width=220;input.appearance.height=160;
        host=take(tree->content_host({},"host",{500,300}));
        good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            auto stack=builder.vstack({},"stack",{.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
            auto value=lvui::native::NativeTable::create(builder,*stack,"table",input,
                {.selection_changed=[&](auto){++selections;},.structure_changed=[&](const auto&){++changes;if(on_change)on_change();}});
            if(!value)return std::unexpected(value.error());table=*value;return {};
        }));
    }
    ~Fixture(){tree->close();}
    Widget find(std::string_view suffix){
        const auto nodes=take(tree->snapshots());const auto node=std::ranges::find_if(nodes,[&](const auto& item){return item.id.ends_with(suffix);});
        if(node==nodes.end())throw std::runtime_error("Missing initial table control: "+std::string(suffix));return take(tree->find(node->id));
    }
    void click(const Widget& widget,double x,double y){const auto r=take(widget.snapshot()).bounds;
        good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=r.x+x,.y=r.y+y,.buttons=1}));
        good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=r.x+x,.y=r.y+y,.buttons=1}));
    }
};
}
int main(){
    using namespace ii::gui;
    {
        Fixture f;auto row=f.find(".first-row"),column=f.find(".first-column");
        auto row_state=take(row.snapshot()),column_state=take(column.snapshot());
        require(row_state.visible&&column_state.visible&&row_state.kind==WidgetKind::action_surface&&column_state.kind==WidgetKind::action_surface&&
                row_state.accessibility_name=="Add first row"&&column_state.accessibility_name=="Add first column"&&
                row_state.action_role==ActionRole::button&&column_state.action_role==ActionRole::button,
                "Empty structure controls lack visible, named action semantics");
        auto gutter=f.find(".row-controls");const auto bounds=take(gutter.snapshot()).bounds;
        const auto drawing=take(f.tree->drawing_snapshot({500,300}));
        const auto matches=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& item){
            return std::abs(item.bounds.x-(bounds.x+6))<.1&&std::abs(item.bounds.y-(bounds.y+12))<.1&&
                   std::abs(item.bounds.width-8)<.1&&std::abs(item.bounds.height-1)<.1;
        });};
        const bool has_plus=matches(drawing.panels)||std::ranges::any_of(drawing.overlays,[&](const auto& layer){return matches(layer.panels);});
        require(has_plus,"A fully empty table did not draw the first-row control");
        good(row.focus());good(f.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::tab}));
        require(take(column.snapshot()).focused,"Keyboard traversal skipped the first-column control");
        good(f.tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+12,.buttons=1}));
        good(f.tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+10,.y=bounds.y+30,.buttons=1}));
        require(take(f.source->snapshot())->revision==0,"A first-row press released outside its slot was committed");
        f.click(gutter,10,12);auto value=take(f.source->snapshot());
        require(value->revision==1&&value->rows.size()==1&&value->rows[0].key=="first-row"&&value->columns.empty()&&
                !take(row.snapshot()).visible&&take(column.snapshot()).visible&&f.changes==1&&f.selections==0,
                "Pointer insertion of the first row changed the wrong state");
        good(column.focus());good(f.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
        require(take(f.source->snapshot())->revision==1,"First column activated on key press");
        good(f.tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));value=take(f.source->snapshot());
        require(value->revision==2&&value->columns.size()==1&&value->columns[0].key=="first-column"&&
                value->rows[0].cells.size()==1&&value->rows[0].cells[0].text=="column"&&
                !take(column.snapshot()).visible&&f.changes==2&&f.selections==0,
                "Keyboard insertion of the first column lost the existing row or changed selection");
    }
    {
        Fixture f({{.key="existing"}});auto row=f.find(".first-row");
        f.factory->invalid=true;good(row.activate());
        require(take(f.source->snapshot())->revision==0&&take(f.table->failure()).has_value()&&take(row.snapshot()).visible&&f.changes==0,
                "An invalid first-row factory changed the document or hid recovery");
        f.factory->invalid=false;f.factory->throws=true;good(row.activate());
        require(take(f.source->snapshot())->revision==0&&take(f.table->failure()).has_value()&&take(row.snapshot()).visible,
                "An exception in the first-row factory escaped or changed the document");
        f.factory->throws=false;good(row.activate());auto value=take(f.source->snapshot());
        require(value->revision==1&&value->rows[0].cells[0].text=="row"&&!take(f.table->failure()).has_value()&&f.changes==1,
                "First-row accessibility activation did not recover or preserve domain defaults");
    }
    {
        Fixture f({},{{.key="existing-row",.cells={}}});auto column=f.find(".first-column"),gutter=f.find(".column-controls");
        require(take(column.snapshot()).visible,"Empty columns did not expose an initial control");
        f.click(gutter,60,10);auto value=take(f.source->snapshot());
        require(value->revision==1&&value->columns[0].key=="first-column"&&value->rows[0].cells[0].text=="column"&&f.changes==1,
                "Pointer insertion of the first column failed with existing rows");
    }
    {
        Fixture f({{.key="existing"}});auto row=f.find(".first-row"),gutter=f.find(".row-controls");
        f.input.add_row_controls_visible=false;good(f.table->set_options(f.input));
        require(!take(row.snapshot()).visible,"A hidden row control remained accessible");
        f.input.add_row_controls_visible=true;f.input.appearance.enabled=false;good(f.table->set_options(f.input));
        require(!take(row.snapshot()).enabled,"A disabled initial button was exposed as enabled");
        good(row.activate());require(take(f.source->snapshot())->revision==0,"A disabled initial action edited the document");
        f.input.appearance.enabled=true;good(f.table->set_options(f.input));
        const auto rect=take(gutter.snapshot()).bounds;
        good(f.tree->input({.kind=WidgetInputKind::pointer_down,.x=rect.x+10,.y=rect.y+12,.buttons=1}));
        (void)take(f.source->replace({{.key="existing"}},{{.key="external",.cells={{}}}},0));
        auto released=f.tree->input({.kind=WidgetInputKind::pointer_up,.x=rect.x+10,.y=rect.y+12,.buttons=1});
        require(!released&&take(f.source->snapshot())->rows.size()==1&&take(f.source->snapshot())->rows[0].key=="external"&&
                !take(row.snapshot()).visible&&f.changes==0,
                "A stale initial action overwrote an external edit or left its button exposed");
    }
    {
        Fixture f({{.key="existing"}});auto row=f.find(".first-row");
        (void)take(f.source->replace({{.key="existing"}},{{.key="external",.cells={{}}}},0));
        good(row.activate());
        require(take(f.source->snapshot())->revision==1&&take(f.source->snapshot())->rows.size()==1&&
                take(f.table->failure()).has_value()&&!take(row.snapshot()).visible&&f.changes==0,
                "Accessibility activation accepted stale data or failed to refresh its control");
    }
    {
        Fixture f;f.input.structure_factory.reset();good(f.table->set_options(f.input));
        auto column=f.find(".first-column"),row=f.find(".first-row");good(column.activate());good(row.activate());
        auto value=take(f.source->snapshot());
        require(value->revision==2&&value->columns[0].key=="lvui-column-1"&&value->rows[0].key=="lvui-row-1"&&
                value->rows[0].cells.size()==1&&f.changes==2,"Default factory failed to initialize an empty table");
    }
    {
        Fixture f;auto row=f.find(".first-row");f.on_change=[&]{good(f.host.clear_content());};
        good(row.activate());require(take(f.source->snapshot())->rows.size()==1&&f.changes==1&&!row.snapshot(),
            "Retiring the table in the first-row callback did not preserve the committed edit");
    }
    {
        Fixture f({{.key="existing"}},{{.key="only-row",.cells={{}}}});auto row=f.find(".first-row"),body=f.find(".body");
        require(!take(row.snapshot()).visible,"A nonempty table exposed the first-row action");
        f.click(body,30,12);require(take(f.input.controller->selection()).has_value(),"A populated cell could not be selected");
        (void)take(f.table->apply_structure({.revision=0,.operations={lvui::TableRemoveRows{{"only-row"}}}}));
        require(take(row.snapshot()).visible&&!take(f.input.controller->selection()).has_value()&&f.selections==2&&f.changes==1,
            "Removing the last row did not clear selection and expose the first-row action");
        good(row.activate());require(take(f.source->snapshot())->rows[0].key=="first-row"&&!take(row.snapshot()).visible&&f.changes==2,
            "The first-row action did not recover after the last row was removed");
    }
}
