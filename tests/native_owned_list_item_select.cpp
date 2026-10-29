#include <LVUI/NativeListItemComposite.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::select;input.row.configuration.label="Status";
    input.selector.options={{"Draft",true},{"Unavailable",false},{"Published",true}};input.selector_index=0;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::optional<native::NativeListItemComposite> item;
    std::vector<int> values;std::vector<std::string> fields;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,
            {.edited=[&](std::string field,const ListItemValue& value){fields.push_back(field);require(std::holds_alternative<std::int64_t>(value),"Selection event has wrong type");},
             .selector_index_changed=[&](int value){values.push_back(value);auto next=take(item->options());next.selector_index=value;good(item->set_options(next));}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto selector=take(item->selector());auto control=take(selector.control());good(control.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(values==std::vector<int>{2}&&fields==std::vector<std::string>{"selectorIndex"}&&take(item->selector_index())==2,
            "Selectable row did not propagate navigation and reactive echo");
    auto next=take(item->options());next.show_selector=false;good(item->set_options(next));
    require(!take(selector.root().interaction()).visible,"Selectable row ignored visibility");
    auto invalid=next;invalid.selector_index=3;require(!item->set_options(invalid)&&take(item->options())==next,"Invalid selection replaced the row");
    good(host.clear_content());require(!item->set_options(input),"Retired selectable row accepted changes");tree->close();
}
