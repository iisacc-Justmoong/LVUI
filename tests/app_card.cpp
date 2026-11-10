#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { AppCard(id: \"card\", title: vm.title, subtitle: vm.subtitle, width: vm.width, shapeStyle: vm.shape, backgroundColor: vm.color) { Label(text: \"Body\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <iiGUI/NativeAppCardView.h>","ii::gui::NativeAppCard::create(","ii::gui::AppCardOptions","component.set_options(value)","->content_root()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated AppCard lost ")+token);
    if(code->find("<LVUI/NativeAppCard.h>")!=code->npos||code->find("lvui::native::NativeAppCard")!=code->npos)
        throw std::runtime_error("Generated AppCard still uses the LVUI visual implementation");
    lvui::NativeCompileOptions options;
    options.style_tokens.overrides["--lvui-theme-colors-panel-surface-solid"]=lvui::StyleColor{.125,.25,.375,1};
    options.style_tokens.overrides["--lvui-theme-metrics-space-18"]=21.;
    options.style_tokens.overrides["--lvui-theme-metrics-radius-12"]=9.;
    options.style_tokens.overrides["--lvui-theme-typography-header-text-header"]=19.;
    auto styled=lvui::compile_native("Window { AppCard() }",options);
    if(!styled||styled->find("value.background = ii::gui::WidgetColor{0.125, 0.25, 0.375, 1}")==styled->npos)
        throw std::runtime_error("AppCard did not take its default surface from the LVUI style profile");
    for(auto token:{"value.padding = 21", "value.corner_radius = 9", "value.title_size = 19"})
        if(styled->find(token)==styled->npos)throw std::runtime_error(std::string("AppCard ignored its style profile: ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated AppCard refers to LVRS");
    for(auto source:{"Window { AppCard(shapeStyle: .bad) }","Window { AppCard(padding: -1) }","Window { AppCard(backgroundColor: \"bad\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid AppCard input was accepted");
}
