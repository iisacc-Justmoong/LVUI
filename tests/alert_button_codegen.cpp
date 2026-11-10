#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { AlertButton(text: vm.text, dialogStyle: true, method: vm.method, onClick: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeAlertButton.h>", "lvui::native::AlertButtonBinding<",
                      "lvui::AlertButtonConfiguration", "lvui::native::ButtonEventValue<ii::gui::Widget>", "component.set_options(value)"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent AlertButton lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default AlertButton depends on a legacy implementation");
    auto motion = lvui::compile_native("import vm Model\nWindow { AlertButton(motionEnabled: vm.motion) }");
    if (!motion || motion->find("value.motion_enabled =") == motion->npos)
        throw std::runtime_error("AlertButton did not forward reactive motionEnabled");
    auto motion_off = lvui::compile_native("Window { AlertButton(motionEnabled: false) }");
    if (!motion_off || motion_off->find("value.motion_enabled = false") == motion_off->npos)
        throw std::runtime_error("AlertButton did not forward static motionEnabled");
    for (auto source : {"Window { AlertButton(tone: .bad) }", "Window { AlertButton(dialogStyle: 1) }",
                        "Window { AlertButton(height: 0) }", "Window { AlertButton(primaryColor: \"bad\") }",
                        "Window { AlertButton(method: 1) }", "Window { AlertButton(motionEnabled: 1) }", "Window { AlertButton { Label() } }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent AlertButton input was accepted");
    }
}
