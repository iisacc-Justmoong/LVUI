#include <LVUI/NativeListItemComposite.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
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
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::stepper;input.row.configuration.label="Copies";
    input.quantity=2;input.minimum_quantity=1;input.maximum_quantity=3;input.step_size=.5;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::optional<native::NativeListItemComposite> item;
    std::vector<double> values;std::vector<std::string> fields;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeListItemComposite::create(builder,*column,"stepper",input,
            {.edited=[&](std::string field,const ListItemValue& value){fields.push_back(field);require(std::holds_alternative<double>(value),"Quantity event has wrong type");},
             .quantity_changed=[&](double value){values.push_back(value);auto next=take(item->options());next.quantity=value;good(item->set_options(next));}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.width==400&&take(item->root().snapshot()).size.height==44&&
            take(item->quantity())==2&&take(item->stepper().interaction()).visible,"Stepper row defaults changed");
    good(item->stepper().focus());
    const auto key=[&](WidgetKey value){good(tree->input({.kind=WidgetInputKind::key_down,.key=value}));};
    key(WidgetKey::up);key(WidgetKey::up);key(WidgetKey::up);
    require(values==std::vector<double>({2.5,3})&&fields==std::vector<std::string>({"quantity","quantity"})&&take(item->quantity())==3,
            "Stepper did not apply its increment, bounds, or reactive echo");
    const auto nodes=take(tree->snapshots());const auto label=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("stepper.quantity");});
    require(label!=nodes.end()&&label->text=="3","The displayed quantity did not follow the model");
    key(WidgetKey::down);require(take(item->quantity())==2.5&&values.back()==2.5,"Stepper decrement was lost");
    auto next=take(item->options());auto bad=next;bad.maximum_quantity=2;
    require(!item->set_options(bad)&&take(item->quantity())==2.5,"An explicit out-of-range quantity replaced the previous state");
    next.quantity.reset();next.maximum_quantity=2;good(item->set_options(next));
    require(take(item->quantity())==2&&values.size()==3,"A range update did not clamp without an edit event");
    next.maximum_quantity=3;next.quantity=2.5;good(item->set_options(next));
    next.stepper_enabled=false;good(item->set_options(next));key(WidgetKey::up);
    require(take(item->quantity())==2.5,"Disabled row stepper changed its value");
    next.show_stepper=false;good(item->set_options(next));
    require(!take(item->stepper().interaction()).visible&&take(take(tree->find(label->id)).interaction()).visible,
            "Hiding the stepper did not preserve the quantity display");
    bad=next;bad.step_size=0;require(!item->set_options(bad)&&take(item->options())==next,"Invalid step size replaced the previous state");
    bad=next;bad.minimum_quantity=4;require(!item->set_options(bad),"Invalid quantity bounds were accepted");
    good(host.clear_content());require(!item->set_options(input),"Retired stepper row accepted an update");tree->close();
}
