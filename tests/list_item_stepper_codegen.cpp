#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Stepper, label: vm.label, quantity: vm.quantity, minimumQuantity: 1, maximumQuantity: 3, stepSize: 0.5, onQuantityChanged: vm.changed, onEdited: vm.edited) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","lvui::native::NativeListItemComposite::create(","lvui::ListItemVariant::stepper",
                   "value.minimum_quantity", "value.maximum_quantity", "value.step_size", ".quantity_changed =", "component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list stepper lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("List stepper depends on legacy widgets");
    for(auto source:{"Window { ListItem(type: .Stepper) }","Window { ListItem(type: .stepper, stepperEnabled: false) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("List stepper was not generated");
    }
    for(auto source:{"Window { ListItem(type: .Stepper, stepSize: 0) }","Window { ListItem(type: .Stepper, quantityWidth: -1) }",
                     "Window { ListItem(type: .Stepper, showStepper: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid list stepper input was accepted");
}
