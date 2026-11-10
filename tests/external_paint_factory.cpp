#include <LVUI/Compiler.h>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
    try {
        constexpr std::string_view source = R"LVUI(
var count: Int = 0
Window { VStack {
    Label(id: "countLabel", text: count)
    PushButton(text: "Increment", onClick: { count = count + 1 })
} }
)LVUI";
        auto view = lvui::compile_native(source, {.name="External", .view_factory_only=true});
        if (!view) throw std::runtime_error(view.error().message);
        check(view->find("static ii::core::Result<void> build(") != view->npos, "Missing native widget factory");
        check(view->find("static ii::gui::WindowGeometry window_geometry()") != view->npos,
              "Factory does not expose initial host geometry");
        check(view->find("widgets.retain(scope)") != view->npos, "Reactive scope lifetime was not retained");
        check(view->find("ii::gui::Host&") == view->npos && view->find("ii::gui::HostOptions") == view->npos &&
              view->find("host.mount_widgets") == view->npos && view->find("#include <iiGUI/Host.h>") == view->npos,
              "Factory-only output still directly depends on Host");
        auto legacy = lvui::compile_native("Window { Label(text: \"OK\") }");
        check(legacy && legacy->find("host.mount_widgets") != legacy->npos, "Existing hosted generation changed");
        auto app = lvui::compile_native("Application { Window { Label(text: \"OK\") } }", {.view_factory_only=true});
        check(app && app->find("struct ApplicationOwner") != app->npos &&
              app->find("build_with_application(") != app->npos && app->find("ii::gui::Host&") == app->npos,
              "A single-scene Application did not expose an independent lifecycle and factory");
        auto multi = lvui::compile_native("Application { Scene(id: \"main\") { Label(text: \"One\") }\nScene(id: \"other\", initiallyOpen: false) { Label(text: \"Two\") } }", {.view_factory_only=true});
        check(!multi && multi.error().message.find("scene") != std::string::npos,
              "Unsupported multi-scene lifecycle was silently discarded");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
