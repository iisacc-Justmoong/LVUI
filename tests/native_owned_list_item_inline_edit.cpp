#include <LVUI/NativeListItemComposite.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::inline_edit;input.row.configuration.label="Name";
    input.input_text="A😀Z";input.primary_action.text="Save";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> values,fields,submissions,actions;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,
            {.edited=[&](std::string field,const ListItemValue& value){fields.push_back(field);require(std::holds_alternative<std::string>(value),"Inline edit has wrong type");},
             .action_triggered=[&](std::string name,const ListFooterSlot& slot){actions.push_back(name+":"+slot.text);},
             .input_text_changed=[&](std::string value){values.push_back(value);auto next=take(item->options());next.input_text=value;good(item->set_options(next));},
             .input_submitted=[&](std::string value){submissions.push_back(std::move(value));}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto field=take(item->input_field());good(field.root().focus());good(field.set_selection(1,3));
    good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(submissions.empty()&&take(field.snapshot()).marked.has_value(),"Uncommitted composition was submitted");
    auto next=take(item->options());next.input_placeholder="Optional";good(item->set_options(next));
    require(take(field.snapshot()).marked.has_value()&&values.empty(),"An unrelated update destroyed the active text composition");
    good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(take(field.snapshot()).text=="A한Z"&&values==std::vector<std::string>{"A한Z"}&&fields==std::vector<std::string>{"inputText1"},
            "Inline editing lost composition, its reactive echo, or the edit event");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(submissions==std::vector<std::string>{"A한Z"},"Inline submission did not report committed text");
    good(take(item->action(0)).activate());require(actions==std::vector<std::string>{"primary:Save"},"Inline primary action did not fire");
    next=take(item->options());next.input_read_only=true;good(item->set_options(next));good(field.root().focus());
    good(tree->input({.kind=WidgetInputKind::text,.text="X"}));require(take(field.snapshot()).text=="A한Z","Read-only inline field changed");
    next.show_input=false;good(item->set_options(next));require(!take(field.root().interaction()).visible,"Inline field did not hide");
    good(host.clear_content());require(!item->set_options(input),"Retired inline item accepted an update");
    unsigned stale=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,
            {.edited=[&](std::string,const ListItemValue&){++stale;},.input_text_changed=[&](std::string){good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto retiring=take(item->input_field());good(retiring.root().focus());good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    require(stale==0&&!item->root().snapshot(),"A retired inline item emitted a stale edit event");tree->close();
}
