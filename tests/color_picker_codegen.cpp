#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

namespace {
void contains(const std::string& code, const char* part) {
    if (code.find(part) == code.npos) throw std::runtime_error(std::string("Independent color picker lost ") + part);
}
void independent(const std::string& code) {
    if (code.find("lvrs::") != code.npos || code.find("<LVRS/") != code.npos || code.find("<Qt") != code.npos)
        throw std::runtime_error("Default color picker depends on a legacy implementation");
}
}
int main() {
    auto owned = lvui::compile_native("Window { ColorPicker(type: .RGB, recentColors: [\"#FF000080\"]) }");
    if (!owned) throw std::runtime_error(owned.error().message);
    for (auto part : {"#include <LVUI/NativeColorPicker.h>", "#include <LVUI/ColorSelectionSurface.h>",
                      "std::make_shared<lvui::ColorSelection>()", "lvui::native::NativeColorPicker::create(",
                      "lvui::ColorPickerMode::rgb", "lvui::StyleColor{1"}) contains(*owned, part);
    independent(*owned);
    auto dynamic = lvui::compile_native("import vm Model\nWindow { ColorPicker(model: vm.session, modelRevision: vm.revision, type: vm.mode, width: vm.width, onColorEdited: vm.edited, onAccepted: vm.accepted) }");
    if (!dynamic) throw std::runtime_error(dynamic.error().message);
    for (auto part : {"lvui::native::NativeColorPicker::create(", "component.set_options(value)",
                      "value.preserve_recent_colors = true", "lvui::native::NativeColorPickerCallbacks"}) contains(*dynamic, part);
    if (dynamic->find("#include <LVUI/ColorSelectionSurface.h>") != dynamic->npos)
        throw std::runtime_error("Injected picker model requires the default adapter");
    independent(*dynamic);
    auto local = lvui::compile_native("import vm Model\nWindow { ColorPicker(width: vm.width) }");
    if (!local) throw std::runtime_error(local.error().message);
    contains(*local, "[scope, color_picker_session_");
    for (auto source : {"Window { ColorPicker(type: .bad) }", "Window { ColorPicker(width: 319) }",
                        "Window { ColorPicker(width: 320, padding: 64) }", "Window { ColorPicker(model: \"bad\") }",
                        "Window { ColorPicker(recentColors: [\"not a color\"]) }", "Window { ColorPicker(modelRevision: -1) }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid color picker input was accepted");
    }
}
