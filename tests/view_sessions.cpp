#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string compile(std::string_view source) {
    auto result = lvui::compile_native(source);
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}
void syntax() {
    const auto code = compile(R"(
Window {
    let step: Int = 1
    var count: Int = 0
    var heading: View { Label(text: count) }
    VStack {
        heading()
        PushButton(text: "+", onClick: { count = count + step })
    }
})");
    check(code.find("assign_value(") != code.npos, "Event did not assign the observable state");
    check(code.find("value_count") != code.npos, "View property did not capture its owner");
}
void diagnostics() {
    for (auto source : {
        "Window { let n: Int = 0\nPushButton(onClick: { n = 1 }) }",
        "Window { PushButton(onClick: { missing = 1 }) }",
        "Window { var n: Int = 0\nPushButton(onClick: { n.field = 1 }) }",
        "Window { var h: View { Label() }\nPushButton(onClick: { h = 1 }) }",
        "Window { var n: Int = 0\nPushButton(onClick: { n = 1; }) }"})
        check(!lvui::compile_native(source), "Invalid session mutation was accepted");
}
void capture() {
    auto code = compile(R"(
var n: Int = 0
let child: View = VStack { PushButton(onClick: { n = n + 1 }) }
Window { VStack { var n: Int = 20
    child()
    PushButton(onClick: { n = n + 2 })
} }
)");
    check(code.find("scope->parent->value_n") != code.npos, "Computed View lost its lexical variable");
}
void queries() {
    const auto code = compile(R"(
import vm QueryBackend
let current: Int = query(vm.readPort, vm.readRequest())
Window { VStack {
    Label(text: current)
    PushButton(onClick: { query(vm.commandPort, vm.commandRequest()) })
} }
)");
    check(code.find("lvui::native::send_query(") != code.npos, "Typed query was not compiled");
    check(compile("import vm QueryBackend\nWindow { Label(text: query(vm.queries(), vm.readRequest())) }")
              .find("lvui::native::send_query(") != std::string::npos,
          "A standard iiSDK query accessor was not compiled");
    check(!lvui::compile_native("Window { Label(text: query(1, 2)) }"), "An invalid query port was accepted");
}
void connections() {
    const auto code=compile(R"(import vm QueryBackend
Window { VStack {
    var value: Int = 0
    QueryConnection(source: vm.events, query: vm.selection, value: "event",
                    onValue: { value = event.amount })
    QueryConnection(source: vm.stream(), query: vm.selection, onValue: vm.changed)
    Label(text: value)
} })");
    check(code.find("NativeQueryConnections.h")!=code.npos&&code.find("query_connections()")!=code.npos&&
          code.find("query_value")!=code.npos,"Query connection or event payload was omitted");
    for(auto source:{
        "Window { QueryConnection() }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, onValue: vm.changed) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection, onValue: vm.changed, capacity: 0) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection, onValue: vm.changed, capacity: 1.5) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection, onValue: vm.changed, overflow: .unlimited) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection, value: \"bad-name\", onValue: { }) } }",
        "import vm Backend\nWindow { VStack { QueryConnection(source: vm.events, query: vm.selection, onValue: vm.changed) { Label() } } }"})
        check(!lvui::compile_native(source),"An invalid query connection was accepted");
    const auto replaced=lvui::compile_native(R"(import vm QueryBackend
Window { for (item in vm.items, key: item.id) { VStack {
    QueryConnection(source: item.events, query: vm.selection, onValue: vm.changed)
    Label(text: item.id)
} } })");
    check(!replaced&&replaced.error().message.find("stable")!=std::string::npos,
          "A replaceable row was accepted as a stable stream owner");
}
void localization() {
    const auto code = compile(R"(Window {
    let title: String = localized("home.title")
    VStack {
        Label(text: title)
        Label(text: localized("home.greeting"))
        PushButton(onClick: setLocale("ko-KR"))
        PushButton(onClick: { setString("home.title", "새 제목") })
    }
})");
    check(code.find("NativeAppStrings.h") != code.npos && code.find("app_strings()") != code.npos &&
          code.find("set_locale(") != code.npos && code.find("set_string(") != code.npos,
          "Localized strings or runtime replacement was not generated");
    for (auto source : {
        "Window { Label(text: localized()) }",
        "Window { Label(text: localized(\"\")) }",
        "Window { Label(text: localized(3)) }",
        "Window { Label(text: localized(\"x\", \"y\")) }",
        "Window { var n: Int = 1\n Label(text: localized(n)) }",
        "Window { PushButton(onClick: setLocale()) }",
        "Window { PushButton(onClick: setLocale(\"ko--KR\")) }",
        "Window { var n: Int = 1\n PushButton(onClick: setLocale(n)) }",
        "Window { PushButton(onClick: { setString(\"\", \"value\") }) }",
        "Window { PushButton(onClick: { setString(\"key\") }) }"})
        check(!lvui::compile_native(source), "Invalid localization expression was accepted");
}
void objects() {
    const auto code = compile(R"(
import vm Backend
Window {
    VStack {
        Label(text: vm.record().title)
        PushButton(onClick: vm.update)
    }
})");
    check(code.find("record()") != code.npos, "Imported C++ object access was not generated");
    check(!lvui::compile_native("Window { let data: Object = { count: 0 }\nPushButton(onClick: { data.count = 1 }) }"),
          "An LVUI object declaration was accepted");
    check(!lvui::compile_native("Window { var data: Object = { count: 0, count: 1 }\nLabel() }"),
          "An LVUI object literal was accepted");
}
void callbacks() {
    const auto code = compile(R"(
let Control(action: () -> Void): View = PushButton(onClick: action)
let Simple: View = PushButton()
Window {
    var count: Int = 0
    VStack {
        Control(action: { count = count + 1 })
        Simple(onClick: { count = count + 2 })
    }
})");
    check(code.find("assign_value(") != code.npos, "Passed event block lost its assignment");
    check(!lvui::compile_native("let C(f: () -> Int): View = Label(text: f())\nWindow { C(f: { query(1, 2) }) }"),
          "An event block was accepted as a value-returning function");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "syntax";
        if (name == "syntax") syntax(); else if (name == "diagnostics") diagnostics();
        else if (name == "capture") capture(); else if (name == "queries") queries();
        else if (name == "objects") objects(); else if(name=="connections") connections();
        else if (name == "callbacks") callbacks(); else if(name=="localization") localization();
        else throw std::runtime_error("Unknown view-session test");
        std::cout << "View sessions " << name << " passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
