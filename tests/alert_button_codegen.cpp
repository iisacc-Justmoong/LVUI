#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { AlertButton(text: vm.text, dialogStyle: true, method: vm.method, onClick: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeAlertButton.h>", "lvui::native::NativeAlertButton::create(",
                      "lvui::AlertButtonConfiguration", "lvui::native::AlertButtonEvent", "component.set_options(value)"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent AlertButton lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default AlertButton depends on a legacy implementation");
    for (auto source : {"Window { AlertButton(tone: .bad) }", "Window { AlertButton(dialogStyle: 1) }",
                        "Window { AlertButton(height: 0) }", "Window { AlertButton(primaryColor: \"bad\") }",
                        "Window { AlertButton(method: 1) }", "Window { AlertButton { Label() } }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent AlertButton input was accepted");
    }
}
