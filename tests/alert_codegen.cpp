#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Alert(title: vm.title, buttonCount: vm.count, button1Method: vm.save, onPrimaryClicked: vm.clicked, motionEnabled: false) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeAlert.h>", "lvui::native::AlertBinding<",
                      "lvui::native::AlertInputValue<ii::gui::RasterImage>", "lvui::native::ButtonEventValue<ii::gui::Widget>", "component.set_options(value)"}) {
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent Alert lost ") + part);
    }
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default Alert depends on a legacy implementation");
    lvui::NativeCompileOptions theme;
    theme.style_tokens.overrides["--lvui-theme-metrics-space-24"]=30.;
    theme.style_tokens.overrides["--lvui-theme-metrics-size-56"]=60.;
    auto themed=lvui::compile_native("Window { Alert() }",theme);
    if(!themed||themed->find("value.configuration.metrics.action_horizontal_inset = 30;")==themed->npos||
       themed->find("value.configuration.metrics.action_height = 60;")==themed->npos)
        throw std::runtime_error("Alert action metrics ignored the chosen design tokens");
    auto initially_open = lvui::compile_native("Window { Alert(id: \"alert\", open: true, motionEnabled: false) }");
    if (!initially_open || initially_open->find("bind_popup_open") == initially_open->npos)
        throw std::runtime_error("Initial Alert open was not compiled");
    if (!lvui::compile_native("import vm Model\nWindow { Alert(open: vm.open) }"))
        throw std::runtime_error("Reactive Alert open was rejected");
    auto empty_icon = lvui::compile_native("Window { Alert(imageSource: \"\", motionEnabled: false) }");
    if (!empty_icon || empty_icon->find("value.use_default_icon = false") == empty_icon->npos)
        throw std::runtime_error("An explicit empty Alert image did not suppress the default icon");
    for (auto source : {"Window { Alert(message: \"a\", description: \"b\") }",
                        "Window { Alert(primaryText: \"a\", button1Text: \"b\") }",
                        "Window { Alert(minWidth: 900, maxWidth: 400) }",
                        "Window { Alert(buttonCount: 4) }", "Window { Alert(buttonCount: 1.5) }",
                        "Window { Alert(shapeStyle: .bad) }", "Window { Alert(primaryColor: \"bad\") }",
                        "Window { Alert { Label() } }", "Window { Alert(open: 12) }",
                        "import vm Model\nWindow { Alert(motion: vm.motion) }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent Alert input was accepted");
    }
}
