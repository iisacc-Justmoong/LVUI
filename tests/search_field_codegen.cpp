#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { SearchField(id: \"search\", text: vm.query, placeholder: vm.placeholder, width: vm.width, enabled: vm.enabled, readOnly: vm.readOnly, clearButtonVisible: vm.clearVisible, accessibilityName: \"Search\", clearAccessibilityName: \"Clear search\", onEdited: vm.edited, onSubmit: vm.submit) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeSearchField.h>","lvui_generated::detail_search_field::NativeSearchField::create(",
                    "lvui::SearchFieldConfiguration","value.clear_button_visible","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated search field lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated search field depends on a legacy control");
    auto cylinder=lvui::compile_native("Window { SearchField(surface: .cylinder) }");
    auto inline_field=lvui::compile_native("Window { SearchField(surface: .inline) }");
    auto reactive=lvui::compile_native("import vm Model\nWindow { SearchField(surface: vm.surface) }");
    if(!cylinder||cylinder->find("value.surface = lvui::InputFieldSurface::cylinder")==cylinder->npos||
       !inline_field||inline_field->find("value.surface = lvui::InputFieldSurface::inline_")==inline_field->npos||
       !reactive||reactive->find("lvui::search_field_surface(")==reactive->npos)
        throw std::runtime_error("Search field surface selection was not generated");
    for(auto source:{"Window { SearchField(width: 0) }","Window { SearchField(enabled: 1) }",
                     "Window { SearchField(backgroundColor: Color.red) }","Window { SearchField(iconName: \"search\") }",
                     "Window { SearchField(surface: .flat) }","Window { SearchField(surface: .unknown) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid or style-overriding search field option was accepted");
}
