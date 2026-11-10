#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Sheet(title: vm.title, preferredWidth: vm.width, motionEnabled: false, onClosed: vm.closed) { Label(text: vm.text) } }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <iiGUI/NativeSheetView.h>", "ii::gui::NativeSheet::create(",
                      "ii::gui::SheetContentFactory", "#include <iiGUI/NativeButtonView.h>",
                      "ii::gui::NativeButton::create(", "lvui::ButtonPalette",
                      "lvui::figma_defaults::panel_surface_panel", "component.set_options(value)",
                      "ii::gui::Widget content"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent Sheet lost ") + part);
    }
    if (code->find("LVUI/NativeSheetAssets.h") != code->npos || code->find("lvui::native::sheet_close_button") != code->npos ||
        code->find("LVUI/NativeButton.h") != code->npos)
        throw std::runtime_error("Sheet still requires an LVUI graphical adapter");
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default Sheet depends on a legacy implementation");
    auto opened = lvui::compile_native("Window { Sheet(id: \"sheet\", open: true, motionEnabled: false) {} }");
    if (!opened || opened->find("bind_popup_open") == opened->npos)
        throw std::runtime_error("Initial Sheet open was not compiled");
    if (!lvui::compile_native("import vm Model\nWindow { Sheet(open: vm.open) {} }"))
        throw std::runtime_error("Reactive Sheet open was rejected");
    for (auto source : {"Window { Sheet(presentation: .bad) {} }", "Window { Sheet(detent: .full) {} }",
                        "Window { Sheet(preferredWidth: -1) {} }", "Window { Sheet(animationDuration: -1) {} }",
                        "Window { Sheet(open: 12) {} }",
                        "import vm Model\nWindow { Sheet(motion: vm.motion) {} }",
                        "Window { VStack { Label(id: \"same\")\nSheet { Label(id: \"same\") } } }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent Sheet input was accepted");
    }
}
