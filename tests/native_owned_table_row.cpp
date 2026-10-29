#include <LVUI/NativeTableRow.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::TableRowInput input;input.configuration.width=350;input.configuration.cell_width=100;input.configuration.inputable=true;
    input.configuration.cells={{"a",{"2","int"}},{"b",{"Second"}},{"c",{"Third"}}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,80}));std::optional<native::NativeTableRow> row;
    std::vector<std::string> edits,submissions,rejections;bool echo=true;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeTableRow::create(builder,*column,"row",input,
            {.edited=[&](std::size_t index,std::string text){edits.push_back(text);if(echo){auto next=take(row->options());next.configuration.cells[index].value.text=text;good(row->set_options(next));}},
             .submitted=[&](std::size_t index,std::string text){submissions.push_back(std::to_string(index)+":"+text);},
             .rejected=[&](std::size_t index,std::string text,std::string){rejections.push_back(std::to_string(index)+":"+text);}});
        if(!value)return std::unexpected(value.error());row=*value;return {};
    }));
    auto first=take(row->cell("a")),second=take(row->cell("b"));
    const auto first_scope=take(first.root().placement()).scope.lock(),second_scope=take(second.root().placement()).scope.lock();
    require(take(row->root().snapshot()).size.width==350&&take(first.root().snapshot()).size.width==100,"Table row did not lay out its cells");
    good(first.begin_edit());
    const auto replace=[&](std::string text){auto field=take(first.editor());auto snapshot=take(field.snapshot());
        good(field.set_selection(0,snapshot.text.size()));good(tree->input({.kind=WidgetInputKind::text,.text=std::move(text)}));};
    replace("bad");good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(rejections==std::vector<std::string>{"0:bad"}&&take(take(first.editor()).snapshot()).text=="bad","Row did not forward a rejected edit");
    replace("12");good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(submissions==std::vector<std::string>{"0:12"}&&take(row->options()).configuration.cells[0].value.text=="12",
            "Row did not publish a valid cell value");
    auto next=take(row->options());next.configuration.cells[1].value.text="Renamed";good(row->set_options(next));
    require(take(take(row->cell("a")).root().placement()).scope.lock()==first_scope&&
            take(take(row->cell("b")).root().placement()).scope.lock()==second_scope&&
            take(take(row->cell("b")).surface().action_options()).accessibility_name=="Renamed",
            "Row did not retain keyed cells during an update");
    std::swap(next.configuration.cells[0],next.configuration.cells[1]);good(row->set_options(next));
    require(take(take(row->cell("a")).root().placement()).scope.lock()==first_scope&&
            take(take(row->cell("b")).root().placement()).scope.lock()==second_scope,"Row rebuilt keyed cells after a reorder");
    next.configuration.width=120;next.offset=100;good(row->set_options(next));
    require(!row->cell("b")&&row->cell("a")&&row->cell("c"),"Row did not retire cells outside its horizontal viewport");
    auto invalid=next;invalid.configuration.cells[1].key=invalid.configuration.cells[0].key;
    require(!row->set_options(invalid)&&take(row->options())==next,"Invalid row changed committed cells");
    bool wrong=false;std::thread worker([&]{auto result=row->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Row accepted a foreign-thread update");
    good(host.clear_content());require(!row->set_options(input),"Retired row accepted an update");
    bool reordered=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeTableRow::create(builder,*column,"reentrant",input,
            {.edited=[&](std::size_t index,std::string text){if(reordered)return;reordered=true;auto next=take(row->options());
                next.configuration.cells[index].value.text=std::move(text);std::swap(next.configuration.cells[0],next.configuration.cells[1]);good(row->set_options(next));}});
        if(!value)return std::unexpected(value.error());row=*value;return {};
    }));
    auto edited=take(row->cell("a"));good(edited.begin_edit());auto field=take(edited.editor());auto snapshot=take(field.snapshot());
    good(field.set_selection(0,snapshot.text.size()));good(tree->input({.kind=WidgetInputKind::text,.text="13"}));
    (void)take(tree->drawing_snapshot({400,80}));
    require(reordered&&take(row->options()).configuration.cells[1].key=="a"&&take(take(row->cell("a")).editor().value().snapshot()).text=="13",
            "Reentrant row reorder lost the retained edit or deferred update");
    good(host.clear_content());tree->close();
}
