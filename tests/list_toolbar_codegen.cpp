#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ListToolbar(icon1: vm.icon, interactive: vm.interactive, onIconClicked: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <iiGUI/NativeListToolbarView.h>", "ii::gui::ListToolbarView::create(",
                      "ii::gui::ListBarInput", "component.set_options(value)"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent list toolbar lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("List toolbar generated a legacy dependency");
    if (code->find("<LVUI/NativeListToolbar.h>") != code->npos || code->find("lvui::native::NativeListToolbar::create(") != code->npos)
        throw std::runtime_error("List toolbar retained its LVUI widget adapter");
    lvui::NativeCompileOptions options;
    options.style_tokens.overrides["Color.subSurface"]=lvui::StyleColor{.1,.2,.3,.4};
    options.style_tokens.overrides["Color.surfaceAlt"]=lvui::StyleColor{.2,.3,.4,.5};
    auto themed=lvui::compile_native("Window { ListToolbar() }",options);
    if(!themed || themed->find("value.configuration.palette.background = ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4}")==themed->npos ||
       themed->find("value.configuration.palette.hover = ii::gui::WidgetColor{0.2, 0.3, 0.4, 0.5}")==themed->npos)
        throw std::runtime_error("List toolbar did not supply application theme tokens to iiGUI");
    for (auto source : {"Window { ListToolbar(iconSize: 0) }", "Window { ListToolbar(buttonSize: -1) }",
                        "Window { ListToolbar(interactive: 1) }", "Window { ListToolbar(iconRevision: 1.5) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid list toolbar configuration was accepted");
}
