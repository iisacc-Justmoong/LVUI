#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { AppHeader(id: \"header\", title: vm.title, subtitle: vm.subtitle, compact: vm.compact, menuVisible: vm.menu_visible, onMenuClicked: vm.menu) { Label(text: \"Action\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <iiGUI/NativeAppHeaderView.h>","ii::gui::NativeAppHeader::create(","ii::gui::AppHeaderOptions","component.set_options(value)","->actions_root()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated AppHeader lost ")+token);
    if(code->find("<LVUI/NativeAppHeader.h>")!=code->npos||code->find("lvui::native::NativeAppHeader")!=code->npos)
        throw std::runtime_error("Generated AppHeader still uses the LVUI visual implementation");
    lvui::NativeCompileOptions options;
    options.style_tokens.overrides["--lvui-theme-colors-panel-window-alt"]=lvui::StyleColor{.125,.25,.375,1};
    options.style_tokens.overrides["--lvui-theme-metrics-space-16"]=21.;
    auto styled=lvui::compile_native("Window { AppHeader() }",options);
    if(!styled||styled->find("value.background = ii::gui::WidgetColor{0.125, 0.25, 0.375, 1}")==styled->npos||
       styled->find("value.normal_metrics.horizontal_padding = 21")==styled->npos||styled->find("value.menu_focus =")==styled->npos)
        throw std::runtime_error("AppHeader did not use its LVUI style and focus inputs");
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated AppHeader refers to LVRS");
    for(auto source:{"Window { AppHeader(compact: 1) }","Window { AppHeader(rowSpacing: -1) }","Window { AppHeader(menuButtonPadding: 4091) }","Window { AppHeader(backgroundColor: \"bad\") }","Window { AppHeader(onMenuClicked: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid AppHeader input was accepted");
}
