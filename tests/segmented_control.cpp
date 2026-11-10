#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { LabelSegmentedControl(forceBorderlessTone: vm.force, spacing: vm.spacing) { LabelButton(text: \"One\", tone: vm.tone) } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeSegmentedControl.h>","#include <iiGUI/NativeSegmentedControlView.h>","lvui::native::SegmentedControlBinding<","lvui::SegmentedControlConfiguration","weak_tone_registration_port()","weak_borderless_tone_port()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated segmented control lost ")+token);
    if(code->find("widgets.label_segmented_control(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated segmented control depends on a prebuilt or legacy control");
    auto icon=lvui::compile_native("import vm Model\nWindow { IconSegmentedControl { IconButton(icon: vm.icon, checkable: true, checked: vm.selected) } }");
    if(!icon||icon->find("lvui::native::SegmentedControlBinding<")==icon->npos ||
       icon->find("lvui::icon_segmented_configuration()") == icon->npos)
        throw std::runtime_error("Icon segmented control did not generate independently");
    for(auto kind:{"LabelSegmentedControl","IconSegmentedControl"}){
        lvui::NativeCompileOptions themed;
        themed.style_tokens.overrides["--lvui-theme-colors-panel-sub-surface"]=lvui::StyleColor{.25,.5,.75,1};
        auto colored=lvui::compile_native("Window { "+std::string(kind)+" }",themed);
        if(!colored || colored->find("value.background = lvui::StyleColor{0.25, 0.5, 0.75, 1};")==colored->npos)
            throw std::runtime_error("Segmented background did not resolve the sub-surface theme token");
        const auto prefix="import vm Model\nWindow { "+std::string(kind)+"(";
        auto methods=lvui::compile_native(prefix+"method: vm.first, methods: [vm.second, [vm.third]]) }");
        if(!methods||methods->find("->set_method(")==methods->npos||methods->find("->set_methods(")==methods->npos)
            throw std::runtime_error("Segmented manual methods were not generated");
        auto dynamic=lvui::compile_native(prefix+"methods: vm.methods) }");
        if(!dynamic||dynamic->find("collect_button_methods<")==dynamic->npos||dynamic->find("component.set_methods(")==dynamic->npos)
            throw std::runtime_error("Reactive segmented methods were not generated");
    }
    auto vertical=lvui::compile_native("import vm Model\nWindow { IconSegmentedControl(orientation: .vertical) { IconButton(icon: vm.icon)\nIconButton(icon: vm.icon) } }");
    if(!vertical || vertical->find("value.orientation = lvui::SegmentedOrientation::vertical") == vertical->npos)
        throw std::runtime_error(vertical ? "Vertical icon segmented control was not generated" : vertical.error().message);
    std::string many="import vm Model\nWindow { IconSegmentedControl(orientation: .vertical) { ";
    for(int i=0;i<15;++i)many+="IconButton(icon: vm.icon)\n";
    many+="} }";
    auto fifteen=lvui::compile_native(many);
    if(!fifteen)throw std::runtime_error("Fifteen icon buttons did not compile: "+fifteen.error().message);
    if(icon->find(".checkable = true")==icon->npos || icon->find(".checked =")==icon->npos || icon->find(".selected =")==icon->npos)
        throw std::runtime_error("Segment button did not preserve the regular checked and accent-selected contract");
    for(auto source:{"Window { LabelSegmentedControl(orientation: .vertical) }", "Window { IconSegmentedControl(orientation: .diagonal) }", "import vm Model\nWindow { IconSegmentedControl(orientation: vm.orientation) }", "Window { LabelSegmentedControl(spacing: -1) }","Window { IconSegmentedControl(shapeStyle: .triangle) }",
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
