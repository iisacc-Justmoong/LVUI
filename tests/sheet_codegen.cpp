#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Sheet(title: vm.title, preferredWidth: vm.width, motionEnabled: false, onClosed: vm.closed) { Label(text: vm.text) } }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeSheet.h>", "lvui::native::NativeSheet::create(",
                      "lvui::native::SheetContentFactory", "component.set_options(value)", "ii::gui::Widget content"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent Sheet lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default Sheet depends on a legacy implementation");
    auto opened = lvui::compile_native("Window { Sheet(id: \"sheet\", open: true, motionEnabled: false) {} }");
    if (!opened || opened->find("mounted->find(\"sheet\")") == opened->npos)
        throw std::runtime_error("Initial Sheet open was not compiled");
    for (auto source : {"Window { Sheet(presentation: .bad) {} }", "Window { Sheet(detent: .full) {} }",
                        "Window { Sheet(preferredWidth: -1) {} }", "Window { Sheet(animationDuration: -1) {} }",
                        "import vm Model\nWindow { Sheet(open: vm.open) {} }",
                        "import vm Model\nWindow { Sheet(motion: vm.motion) {} }",
                        "Window { VStack { Label(id: \"same\")\nSheet { Label(id: \"same\") } } }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent Sheet input was accepted");
    }
}
