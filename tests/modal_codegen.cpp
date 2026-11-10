#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Modal(title: vm.title, onPrimaryClicked: vm.save, motionEnabled: false) { Label(text: vm.text) } }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeModal.h>", "lvui::native::ModalBinding<",
                      "lvui::native::ModalContentFactoryValue<ii::gui::WidgetBuilder,ii::gui::Widget>", "component.set_options(value)", "ii::gui::Widget content"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent Modal lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default Modal depends on a legacy implementation");
    auto initially_open = lvui::compile_native("Window { Modal(id: \"modal\", open: true, motionEnabled: false) {} }");
    if (!initially_open || initially_open->find("bind_popup_open") == initially_open->npos)
        throw std::runtime_error("Initial Modal open was not compiled");
    for (auto source : {"Window { Modal { Modal(open: true) {} } }", "import vm Model\nWindow { Modal(open: vm.open) {} }"})
        if (!lvui::compile_native(source)) throw std::runtime_error("Reactive or nested Modal open was rejected");
    for (auto source : {"Window { Modal(desc: \"a\", description: \"b\") {} }",
                        "Window { Modal(minWidth: 900, maxWidth: 400) {} }",
                        "Window { Modal(buttonCount: 4) {} }", "Window { Modal(buttonCount: 1.5) {} }",
                        "Window { Modal(shapeStyle: .bad) {} }", "Window { Modal(initialScale: .1) {} }",
                        "Window { Modal { Label()\nLabel() } }", "Window { Modal(open: 12) {} }",
                        "import vm Model\nWindow { Modal(motion: vm.motion) {} }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent Modal input was accepted");
    }
}
