#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { LabelSegmentedControl(forceBorderlessTone: vm.force, spacing: vm.spacing) { LabelButton(text: \"One\", tone: vm.tone) } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeSegmentedControl.h>","lvui::native::NativeSegmentedControl::create(","lvui::SegmentedControlConfiguration","weak_tone_registration_port()","weak_borderless_tone_port()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated segmented control lost ")+token);
    if(code->find("widgets.label_segmented_control(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated segmented control depends on a prebuilt or legacy control");
    auto icon=lvui::compile_native("import vm Model\nWindow { IconSegmentedControl { IconButton(icon: vm.icon) } }");
    if(!icon||icon->find("lvui::native::NativeSegmentedControl::create(")==icon->npos)
        throw std::runtime_error("Icon segmented control did not generate independently");
    for(auto source:{"Window { LabelSegmentedControl(spacing: -1) }","Window { IconSegmentedControl(shapeStyle: .triangle) }",
                     "Window { LabelSegmentedControl(forceBorderlessTone: 1) }","Window { LabelSegmentedControl(currentIndex: 0) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid segmented control input was accepted");
    auto nested=lvui::compile_native(R"(import vm Model
Window {
    LabelSegmentedControl {
        VStack { LabelButton(text: "One") }
        if (vm.show) { LabelButton(text: "Two") }
        for (item in vm.items, key: item.id) { LabelButton(text: item.label) }
    }
})");
    if(!nested)throw std::runtime_error(nested.error().message);
    if(nested->find("scope->parent->segmented_style_")==nested->npos)
        throw std::runtime_error("Segmented child style did not cross nested activations");
}
