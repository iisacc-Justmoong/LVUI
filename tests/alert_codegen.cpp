#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Alert(title: vm.title, buttonCount: vm.count, button1Method: vm.save, onPrimaryClicked: vm.clicked, motionEnabled: false) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeAlert.h>", "lvui::native::NativeAlert::create(",
                      "lvui::native::AlertInput", "lvui::native::AlertButtonEvent", "component.set_options(value)"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent Alert lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default Alert depends on a legacy implementation");
    auto initially_open = lvui::compile_native("Window { Alert(id: \"alert\", open: true, motionEnabled: false) }");
    if (!initially_open || initially_open->find("mounted->find(\"alert\")") == initially_open->npos)
        throw std::runtime_error("Initial Alert open was not compiled");
    auto empty_icon = lvui::compile_native("Window { Alert(imageSource: \"\", motionEnabled: false) }");
    if (!empty_icon || empty_icon->find("value.use_default_icon = false") == empty_icon->npos)
        throw std::runtime_error("An explicit empty Alert image did not suppress the default icon");
    for (auto source : {"Window { Alert(message: \"a\", description: \"b\") }",
                        "Window { Alert(primaryText: \"a\", button1Text: \"b\") }",
                        "Window { Alert(minWidth: 900, maxWidth: 400) }",
                        "Window { Alert(buttonCount: 4) }", "Window { Alert(buttonCount: 1.5) }",
                        "Window { Alert(shapeStyle: .bad) }", "Window { Alert(primaryColor: \"bad\") }",
                        "Window { Alert { Label() } }", "import vm Model\nWindow { Alert(open: vm.open) }",
                        "import vm Model\nWindow { Alert(motion: vm.motion) }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent Alert input was accepted");
    }
}
