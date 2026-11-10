#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { ListItem(type: .DetailQuantity, quantity: vm.quantity, stepperArrow: .Down, stepperTone: .disabled, unitSelector: [\"px\", \"cm\"], unitIndex: vm.unit, onUnitIndexChanged: vm.unitChanged)\nListItem(type: .DetailSettings, segments: [\"Original\", \"Edited\"], segmentIndex: vm.segment, onSegmentIndexChanged: vm.segmentChanged) } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"lvui::ListItemVariant::detail_quantity","lvui::ListItemVariant::detail_settings","value.unit_selector",
                   "value.unit_index","value.segments","value.segment_index",".unit_index_changed", ".segment_index_changed",
                   "value.primary_action.text = \"Apply\"","value.secondary_action.text = \"Reset\"",
                   "value.stepper_arrow = lvui::StepperArrow::down","value.stepper_tone = lvui::ButtonTone::disabled"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Detail control generation lost ")+part);
    for(auto source:{"Window { ListItem(type: .detailQuantity) }","Window { ListItem(type: .detailSettings) }"}){
        auto item=lvui::compile_native(source);if(!item||item->find("NativeListItemComposite::create")==item->npos)
            throw std::runtime_error("A detail control did not select the compound runtime");
    }
    for(auto source:{"Window { ListItem(type: .DetailQuantity, unitSelector: [1]) }",
                     "Window { ListItem(type: .DetailSettings, segments: [false]) }",
                     "Window { ListItem(type: .DetailQuantity, stepperArrow: .Sideways) }",
                     "Window { ListItem(type: .DetailQuantity, unitIndex: -1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid detail control input was accepted");
}
