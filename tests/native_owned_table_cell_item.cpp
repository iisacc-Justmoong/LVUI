#include <LVUI/NativeTableCellItem.h>
#include <iiGUI/WidgetState.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Validator final:lvui::TableCellValueValidator {
    mutable std::function<void()> on_validate;
    lvui::TableResult<void> validate(std::string_view value,std::string_view)const override {
        if(on_validate)on_validate();if(value=="yes")return {};
        return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Expected yes"});
    }
};
}
int main(){
    using namespace lvui;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,80}));
    native::TableCellItemInput input;input.configuration.text="2";input.configuration.value_type="int";input.configuration.inputable=true;
    std::optional<native::NativeTableCellItem> cell;std::vector<std::string> drafts,submitted,rejected;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeTableCellItem::create(builder,*column,"cell",input,
            {.edited=[&](std::string value){drafts.push_back(std::move(value));},
             .submitted=[&](std::string value){submitted.push_back(std::move(value));},
             .rejected=[&](std::string value,std::string){rejected.push_back(std::move(value));}});
        if(!value)return std::unexpected(value.error());cell=*value;return {};
    }));
    require(take(cell->root().snapshot()).size.width==234&&take(cell->root().snapshot()).size.height==24,"Standalone cell dimensions changed");
    const auto replace=[&](std::string text){auto field=take(cell->editor());auto snapshot=take(field.snapshot());
        good(field.set_selection(0,snapshot.text.size()));good(tree->input({.kind=WidgetInputKind::text,.text=std::move(text)}));};
    const auto enter=[&]{good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));};
    good(cell->begin_edit());replace("bad");enter();
    require(rejected==std::vector<std::string>{"bad"}&&take(take(cell->editor()).snapshot()).text=="bad"&&
            take(cell->options()).configuration.text=="2","Invalid cell edit did not retain its draft");
    replace("12");enter();
    require(submitted==std::vector<std::string>{"12"}&&!cell->editor()&&take(cell->options()).configuration.text=="12"&&drafts.back()=="12",
            "Valid cell edit was not committed once");
    good(cell->begin_edit());replace("15");auto changed=take(cell->options());changed.configuration.text="99";good(cell->set_options(changed));enter();
    require(take(take(cell->editor()).snapshot()).text=="15"&&take(cell->options()).configuration.text=="99"&&rejected.back()=="15",
            "External cell change did not preserve and reject the stale draft");
    good(cell->cancel_edit());require(!cell->editor(),"Canceled cell edit stayed open");
    good(cell->begin_edit());good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(take(take(cell->editor()).snapshot()).marked.has_value()&&!cell->commit_edit(),"Uncommitted composition was submitted");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(!cell->editor()&&take(cell->options()).configuration.text=="99","Escape did not cancel the cell draft");
    changed.configuration.enabled=false;good(cell->set_options(changed));require(!cell->begin_edit(),"Disabled cell began editing");
    bool wrong=false;std::thread worker([&]{auto result=cell->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Cell accepted a foreign-thread update");
    good(host.clear_content());require(!cell->set_options(input),"Retired cell accepted an update");
    auto validator=std::make_shared<Validator>();native::TableCellItemInput custom;
    custom.configuration.text="no";custom.configuration.value_type="custom";custom.configuration.inputable=true;custom.validator=validator;
    unsigned echo=0,after_remove=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeTableCellItem::create(builder,{},"custom",custom,
            {.edited=[&](std::string value){++echo;auto options=take(cell->options());options.configuration.text=std::move(value);good(cell->set_options(options));},
             .submitted=[&](std::string){++after_remove;good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());cell=*value;return {};
    }));
    good(cell->begin_edit());replace("yes");require(echo==1&&take(cell->options()).configuration.text=="yes","Reactive draft echo was lost");
    validator->on_validate=[&]{auto options=take(cell->options());options.configuration.width=250;good(cell->set_options(options));};
    require(!cell->commit_edit()&&bool(cell->editor()),"A reentrant validator committed stale cell state");
    validator->on_validate={};good(cell->commit_edit());
    require(after_remove==1&&!cell->set_options(custom),"Submission callback did not retire the cell safely");tree->close();
}
