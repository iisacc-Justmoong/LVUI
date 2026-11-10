#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { InputField(id: \"field\", text: vm.text, placeholder: vm.placeholder, width: vm.width, enabled: vm.enabled, readOnly: vm.readOnly, password: vm.password, multiline: true, wrapEnabled: vm.wrap, maximumLength: vm.maxLength, onEdited: vm.edited, onSubmit: vm.submit) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeInputField.h>","#include <iiGUI/NativeInputFieldView.h>","lvui_generated::detail_input::NativeInputField::create(","lvui::InputFieldConfiguration","value.wrap_enabled","value.maximum_length","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated input field lost ")+token);
    if(code->find("widgets.input_field(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated input field depends on a prebuilt or legacy control");
    auto defaults=lvui::compile_native("Window { InputField() }");
    auto zero=lvui::compile_native("Window { InputField(maximumLength: 0) }");
    if(!defaults||defaults->find("value.maximum_length = 32767;")==defaults->npos||
       !zero||zero->find("value.maximum_length = 0;")==zero->npos)
        throw std::runtime_error("Input field did not preserve default and zero limits");
    auto compact=lvui::compile_native("Window { InputField(size: .compact) }");
    auto reactive=lvui::compile_native("import vm Model\nWindow { InputField(size: vm.size) }");
    if(!compact||compact->find("lvui::apply_input_field_size(value, lvui::InputFieldSize::compact)")==compact->npos||
       !reactive||reactive->find("lvui::input_field_size(")==reactive->npos)
        throw std::runtime_error("Input field size preset was not generated");
    auto explicit_height=lvui::compile_native("Window { InputField(size: .compact, height: 40) }");
    if(!explicit_height||explicit_height->find("lvui::apply_input_field_size(")==explicit_height->npos||
       explicit_height->find("value.height = 40")==explicit_height->npos||
       explicit_height->find("lvui::apply_input_field_size(")>=explicit_height->find("value.height = 40"))
        throw std::runtime_error("An explicit input height did not follow its preset");
    auto clear=lvui::compile_native("import vm Model\nWindow { InputField(clearButtonVisible: vm.clear, clearAccessibilityName: vm.clearName) }");
    if(!clear||clear->find("value.clear_button_visible")==clear->npos||clear->find("value.clear_accessibility_name")==clear->npos)
        throw std::runtime_error("Input clear options were not generated");
    auto rounded=lvui::compile_native("Window { InputField(surface: .rounded) }");
    auto cylinder=lvui::compile_native("Window { InputField(surface: .cylinder) }");
    auto inline_field=lvui::compile_native("Window { InputField(surface: .inline) }");
    auto reactive_surface=lvui::compile_native("import vm Model\nWindow { InputField(surface: vm.surface) }");
    if(!rounded||rounded->find("value.surface = lvui::InputFieldSurface::rounded")==rounded->npos||
       !cylinder||cylinder->find("value.surface = lvui::InputFieldSurface::cylinder")==cylinder->npos||
       !inline_field||inline_field->find("value.surface = lvui::InputFieldSurface::inline_")==inline_field->npos||
       !reactive_surface||reactive_surface->find("lvui::input_field_surface(")==reactive_surface->npos)
        throw std::runtime_error("Input surface selection was not generated");
    for(auto source:{"Window { InputField(width: 0) }","Window { InputField(height: 0) }",
                     "Window { InputField(fontSize: 0) }","Window { InputField(enabled: 1) }",
                     "Window { InputField(backgroundColor: \"invalid\") }",
                     "Window { InputField(maximumLength: -1) }", "Window { InputField(maximumLength: 1.5) }",
                     "Window { InputField(maximumLength: 2147483648) }", "Window { InputField(size: .huge) }",
                     "Window { InputField(size: 1.5) }","Window { InputField(clearButtonVisible: 1) }",
                     "Window { InputField(surface: .unknown) }","Window { InputField(surface: 1.5) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid input field option was accepted");
}
