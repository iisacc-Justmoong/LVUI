#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ToolbarButton(buttonId: vm.id, iconName: vm.icon, text: vm.text, visible: vm.visible, onClick: vm.clicked, onTriggered: vm.triggered) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeToolbarButton.h>", "lvui::native::NativeToolbarButton::create(",
                      "lvui::native::ToolbarButtonInput", "component.set_options(value)", "lvui::native::toolbar_button_id(",
                      "const lvui::ToolbarButtonId& value"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent toolbar button lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default toolbar button depends on legacy widgets");
    auto empty = lvui::compile_native("Window { ToolbarButton() }");
    if (!empty || empty->find("NativeToolbarButton::create") == empty->npos) throw std::runtime_error("Default toolbar button was not generated");
    for (auto source : {"Window { ToolbarButton(buttonId: 1.5) }", "Window { ToolbarButton(buttonId: \"\") }",
                        "Window { ToolbarButton(iconSize: -1) }", "Window { ToolbarButton(tone: .unknown) }",
                        "Window { ToolbarButton(iconName: \"home\", icon: 1) }", "Window { ToolbarButton { Label(text: \"bad\") } }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent toolbar button input was accepted");
}
