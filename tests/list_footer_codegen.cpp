#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ListFooter(button1: vm.first, interactive: vm.enabled, onButtonClicked: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeListFooter.h>", "lvui::native::NativeListFooter::create(",
                      "lvui::native::ListFooterInput", "lvui::native::list_footer_slot(", "component.set_options(value)",
                      "const lvui::ListFooterSlot& slot"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent list footer lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default list footer depends on legacy widgets");
    auto empty = lvui::compile_native("Window { ListFooter() }");
    if (!empty || empty->find("NativeListFooter::create") == empty->npos) throw std::runtime_error("Default list footer was not generated");
    for (auto source : {"Window { ListFooter(button1: 1) }", "Window { ListFooter(iconSize: -1) }",
                        "Window { ListFooter(iconRevision: 1.5) }", "Window { ListFooter { Label(text: \"bad\") } }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent footer input was accepted");
}
