#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { CheckBox(text: vm.text, checked: vm.checked, enabled: vm.enabled, boxSize: vm.size, checkedColor: vm.color, shapeStyle: vm.shape, motionEnabled: vm.animated, motion: vm.motion, transitionDuration: vm.duration, useFigmaCheckedAssets: vm.useAsset, innerShadowSoftColor: vm.shadow, innerShadowStrongColor: vm.shadow, onToggled: vm.changed) }");
    require(bool(code),"Independent checkbox did not compile");
    require(code->find("#include <LVUI/NativeCheckBox.h>")!=code->npos&&
            code->find("lvui::native::CheckBoxBinding<ii::gui::BasicCheckBox, ii::gui::BasicNativeCheckBox, ii::gui::CachedSvgDecoder>::create(")!=code->npos&&
            code->find("#include <iiGUI/NativeCheckBoxView.h>")!=code->npos&&
            code->find("lvui::CheckBoxConfiguration{")!=code->npos&&
            code->find("component.set_options(value)")!=code->npos&&code->find(".use_figma_checked_assets")!=code->npos&&
            code->find(".inner_shadow_soft")!=code->npos&&code->find(".inner_shadow_strong")!=code->npos&&code->find(".shape_style")!=code->npos&&
            code->find(".motion_enabled")!=code->npos&&code->find("checkbox_motion_duration")!=code->npos&&
            code->find("widgets.check_box(")==code->npos&&code->find("lvrs::")==code->npos,
            "Checkbox application composition is missing");
    auto invalid=lvui::compile_native("Window { CheckBox(boxSize: -1) }");
    require(!invalid&&invalid.error().line>0,"Invalid static checkbox size was accepted");
    require(!lvui::compile_native("Window { CheckBox(useFigmaCheckedAssets: \"invalid\") }"),"Invalid asset override was accepted");
    require(!lvui::compile_native("Window { CheckBox(transitionDuration: 1.5) }")&&
            !lvui::compile_native("Window { CheckBox(motion: 3) }"),"Invalid checkbox motion was accepted");
    require(lvui::compile_native("Window { VStack {\n CheckBox(shapeStyle: .cylinder)\n CheckBox(shapeStyle: .roundRect)\n CheckBox(shapeStyle: 1)\n } }")&&
            !lvui::compile_native("Window { CheckBox(shapeStyle: .unknown) }")&&
            !lvui::compile_native("Window { CheckBox(shapeStyle: 2) }"),"Checkbox shape validation failed");
}
