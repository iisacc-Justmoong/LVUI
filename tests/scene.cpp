#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string compile(std::string_view source) {
    auto result = lvui::compile_native(source);
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}
void syntax() {
    const auto* keyword = lvui::find_keyword("Scene");
    require(keyword && keyword->kind == lvui::KeywordKind::view, "Scene is not registered");
    const auto code = compile(R"(
Application {
    Scene(id: "main", title: "Workspace") {
        var count: Int = 0
        var destination: String = "legacy"
        VStack {
            Label(text: count)
            PushButton(onClick: { count = count + 1 })
            PushButton(onClick: {
                destination = "preferences"
                openScene(destination)
                destination = "legacy"
            })
            PushButton(onClick: { closeWindow("legacy") })
        }
    }
    Scene(id: "preferences", initiallyOpen: false) {
        PushButton(onClick: closeScene("preferences"))
    }
    Window(id: "legacy") { Label() }
})");
    require(code.find("LVUI/Scene.h") != code.npos, "Scene descriptor header is missing");
    require(code.find("SceneDescriptor") != code.npos && code.find("scenes{") != code.npos,
            "Scene descriptors were not emitted");
    require(code.find("assign_value(") != code.npos && code.find("scene_registry") != code.npos,
            "Scene state or commands did not connect to existing runtime");
    require(code.find("<Qt") == code.npos && code.find("<LVRS/") == code.npos,
            "Scene introduced a Qt or LVRS dependency");
    const auto markup = compile(R"(Application { Scene(id: "main") { Label(text: "Hello") } })");
    require(markup.find("SceneDescriptor") != markup.npos, "Scene declaration was not compiled");
    const auto localized_counter = compile(R"(
Application {
    Scene(title: localized("window.title")) {
        var count: Int = 0
        VStack {
            Label(text: localized("counter.heading"))
            Label(text: count)
            PushButton(text: localized("counter.increment"), onClick: { count = count + 1 })
            PushButton(text: localized("language.korean"), onClick: setLocale("ko-KR"))
            PushButton(text: localized("counter.replace"), onClick: {
                setString("counter.heading", localized("counter.replacement"))
            })
        }
    }
})");
    require(localized_counter.find("SceneDescriptor") != localized_counter.npos &&
            localized_counter.find("assign_value(") != localized_counter.npos &&
            localized_counter.find("->set_locale(") != localized_counter.npos &&
            localized_counter.find("->set_string(") != localized_counter.npos &&
            localized_counter.find("counter.heading") != localized_counter.npos &&
            localized_counter.find("counter.replacement") != localized_counter.npos,
            "Declarative Scene lost state or localization bindings");
    const auto explicit_counter = compile(R"(
Application() {
    Scene(title: localized("window.title")) {
        var count: Int = 0,
        VStack() {
            Label() { text: localized("counter.heading") },
            Label() { text: count },
            PushButton() { text: localized("counter.increment"), onClick: { count = count + 1 } },
            PushButton() { text: localized("language.korean"), onClick: setLocale("ko-KR") },
            PushButton() { text: localized("counter.replace"), onClick: {
                setString("counter.heading", localized("counter.replacement"))
            } },
        },
    },
})");
    require(explicit_counter == localized_counter,
            "Explicit constructor and comma-separated View arguments changed native output");
    require(!lvui::compile_native("<Application><Scene><Label/></Scene></Application>"),
            "HTML-like markup was accepted as standard LVUI source");
    const auto dynamic = compile(R"(
Application {
    import vm SceneSelector
    Scene { PushButton(onClick: openScene(vm.target)) }
    Scene(id: "tools", initiallyOpen: false) { Label() }
})");
    require(dynamic.find("selected_window_id") != dynamic.npos, "Dynamic Scene target was not captured");
    const auto callbacks = compile(R"(
Application {
    Scene { VStack {
        PushButton(onClick: { openScene("tools") })
        PushButton(onClick: openScene("tools"))
    } }
    Scene(id: "tools", initiallyOpen: false) { Label() }
})");
    require(callbacks.find("scene_registry") != callbacks.npos, "An event callback lost its Scene command");
}
void diagnostics() {
    for (const auto* source : {
        "Scene { Label() }",
        "Application { Scene {} }",
        "Application { Scene { Label()\nLabel() } }",
        "Application { Scene(id: \"same\") { Label() }\nWindow(id: \"same\") { Label() } }",
        "Application { Scene(id: \"main\", initiallyOpen: false) { Label() } }",
        "Application { Scene(id: \"single\") { PushButton(onClick: openScene(\"single\")) } }",
        "Window { PushButton(onClick: { openScene(\"tools\") }) }",
        "Application { Scene { PushButton(onClick: { openScene(\"missing\") }) }\nScene(id: \"tools\") { Label() } }",
        "Application { Scene { Window { Label() } } }",
        "Application { Scene(wrong: true) { Label() } }",
        "Application { Scene { PushButton(onClick: openScene(\"missing\")) }\nScene(id: \"other\") { Label() } }",
        "Application { Window { Scene { Label() } } }"})
        require(!lvui::compile_native(source), "Invalid Scene declaration was accepted");
}
}
int main() { syntax(); diagnostics(); }
