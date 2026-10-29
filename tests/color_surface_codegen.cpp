#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ColorPickerSurface(model: vm.source, modelRevision: vm.revision, field: .HueRail, onColorEdited: vm.edited) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeColorSurface.h>", "lvui::native::NativeColorSurface::create(",
                      "lvui::native::ColorSurfaceInput", "lvui::ColorSurfaceKind::hue_rail", "component.set_options(value)"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent color surface lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default color surface depends on a legacy implementation");
    for (auto source : {"Window { ColorField() }", "Window { ColorField(model: \"wrong\") }",
                        "import vm Model\nWindow { ColorPickerSurface(model: vm.source, field: .Unknown) }",
                        "import vm Model\nWindow { ColorPickerSurface(model: vm.source, width: 11) }",
                        "import vm Model\nWindow { ColorPickerSurface(model: vm.source, markerDiameter: 65) }",
                        "import vm Model\nWindow { ColorPickerSurface(model: vm.source, modelRevision: -1) }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid color surface input was accepted");
    }
}
