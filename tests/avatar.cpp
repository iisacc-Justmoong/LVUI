#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Avatar(id: \"avatar\", size: vm.size, source: vm.image, width: vm.width, backgroundColor: vm.color, accessibilityName: vm.name) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <iiGUI/NativeAvatarView.h>","ii::gui::NativeAvatar::create(","ii::gui::AvatarInput","ii::gui::make_avatar_placeholder_source(","lvui::native::avatar_placeholder_mask_catalog()","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Avatar lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated Avatar refers to LVRS");
    auto address=lvui::compile_native("Window { Avatar(source: \"avatar.png\") }");
    if(!address||address->find("resolve_scoped_image<ii::gui::RasterImage>(")==address->npos)throw std::runtime_error("Avatar URL did not use its scoped image service");
    lvui::NativeCompileOptions theme;
    theme.style_tokens.overrides["Color.text.body"]=lvui::StyleColor{.25,.5,.75,1};
    theme.style_tokens.overrides["Color.surfaceRaised"]=lvui::StyleColor{.1,.2,.3,.4};
    auto themed=lvui::compile_native("Window { Avatar() }",theme);
    if(!themed||themed->find("value.placeholder_color = ii::gui::avatar_color(ii::gui::WidgetColor{0.25, 0.5, 0.75, 1})")==themed->npos)
        throw std::runtime_error("Avatar placeholder ignored the selected body color token");
    if(themed->find("value.configuration.background = ii::gui::avatar_color(ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})")==themed->npos)
        throw std::runtime_error("Avatar background ignored the selected surface color token");
    auto explicit_background=lvui::compile_native("Window { Avatar(backgroundColor: \"#ff0000\") }",theme);
    if(!explicit_background||explicit_background->find("value.configuration.background = ii::gui::avatar_color(ii::gui::WidgetColor{1, 0, 0, 1})")==explicit_background->npos||
       explicit_background->find("value.configuration.background = ii::gui::avatar_color(ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})")!=explicit_background->npos)
        throw std::runtime_error("An explicit Avatar background did not override the shared token");
    for(auto source:{"Window { Avatar(size: .bad) }","Window { Avatar(width: -1) }","Window { Avatar(source: 123) }",
                     "Window { Avatar(backgroundColor: \"bad\") }","Window { Avatar { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Avatar input was accepted");
}
