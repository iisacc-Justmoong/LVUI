#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string compile(std::string_view text, std::string framework = "iiGUI") {
    auto result = lvui::compile_native(text, {.framework = std::move(framework)});
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}
void syntax() {
    auto result = lvui::parse_view("let panel: View = VStack { Label(text: \"one\") }\nlet copied = panel\nWindow { copied() }");
    check(result && result->bindings()[0].initializer.kind == lvui::ValueKind::view, "View declaration was not retained");
    check(result->bindings()[0].initializer.view->component->spelling == "VStack", "Visual initializer was lost");
    check(result->bindings()[1].type == "View" && result->root().children[0].custom_component == "copied",
          "View alias or lexical invocation was lost");
    auto inferred = lvui::parse_view("let tiny = Label()\nWindow { tiny() }");
    check(inferred && inferred->bindings()[0].type == "View", "A visual initializer was not inferred as View");
}
void captured() {
    auto code = compile("import vm Backend\nlet heading: View = Label(text: vm.title)\nWindow {\nimport vm OtherBackend\nVStack { heading()\nLabel(text: vm.title) }\n}");
    check(code.find("scope->backend_0.title, scope->revision_0") != code.npos &&
          code.find("scope->backend_1.title, scope->revision_1") != code.npos,
          "A View value captured the caller's shadowed backend");
    auto nested = compile("import vm Backend\nlet heading: View = Label(text: vm.title)\nWindow { VStack { let local = 2\nheading() } }");
    check(nested.find("scope->parent->backend_0.title") != nested.npos, "A nested View value did not borrow its lexical parent");
    auto shadowed = compile("let heading: View = Label(text: \"outer\")\nWindow { let heading: View = Label(text: \"inner\")\nVStack { heading() } }");
    check(shadowed.find("inner") != shadowed.npos && shadowed.find("outer") == shadowed.npos, "Local View shadowing failed");
}
void identity() {
    auto code = compile("let tile: View = VStack(id: \"tile\") { Label(id: \"label\", text: \"content\") }\nWindow { VStack { tile()\ntile() } }");
    for (auto id : {"tile@0/tile", "tile@0/label", "tile@1/tile", "tile@1/label"})
        check(code.find(id) != code.npos, "Repeated View values share explicit child IDs");
    auto typed = compile("let item: View = Tab(iconName: \"generaladd\")\nWindow { TabBar { item()\nitem() } }", "LVRS");
    check(typed.find("controls.tab(") != typed.npos, "A typed child slot did not resolve a View value");
    auto targets = compile("let box: View = VStack { Label(id: \"target\")\nTooltip(targetId: \"target\", text: \"help\") }\nWindow { VStack { box()\nbox() } }", "LVRS");
    check(targets.find("->set_target(*") != targets.npos, "View-local tooltip targets were lost");
}
void root() {
    const auto code = compile("let app: View = Window(width: 720) { Label(text: \"compiled app\") }\napp()");
    check(code.find(".size = {720, 420}") != code.npos && code.find("compiled app") != code.npos,
          "Window-valued root was not expanded at compile time");
    check(code.find("parse_view") == code.npos && code.find("ViewDefinition") == code.npos,
          "A View value shipped compiler AST or source evaluation");
}
void inputs_and_slots() {
    auto code = compile("import vm Backend\nlet box: View = VStack { import vm PrivateBackend\nLabel(text: vm.title) }\nlet label: View = Label(text: \"default\")\nlet button: View = PushButton(vm.save, text: \"Default\")\nWindow { import vm CallerBackend\nVStack {\nlabel(text: vm.title)\nbutton(text: \"Other\", onClick: vm.save)\nbox() { Label(text: vm.title) }\n} }");
    check(code.find("scope->parent->backend_1.title") != code.npos,
          "A supplied child slot read the prototype's private backend");
    check(code.find("object).save(std::forward") != code.npos && code.find("Other") != code.npos,
          "A supplied callback or text input was not captured at its call site");
    auto constant = compile("let content = \"caller\"\nlet label: View = Label(text: \"default\")\nWindow { label(text: content) }");
    check(constant.find("\"caller\"") != constant.npos && constant.find("\"default\"") == constant.npos,
          "A constant input override was not used");
    auto application = compile("let app: View = Window { Label(text: \"default\") }\napp(width: 900) { Label(text: \"replacement\") }");
    check(application.find(".size = {900, 420}") != application.npos && application.find("replacement") != application.npos,
          "A Window-valued composition did not accept root inputs and content");
    auto typed = compile("let bar: View = TabBar()\nWindow { let item: View = Tab(iconName: \"generaladd\")\nbar() { item() } }", "LVRS");
    check(typed.find("controls.tab(") != typed.npos,"A supplied typed child did not use the caller's View definitions");
    const auto quoted = compile("import vm Backend\nlet item: View = Label() { import vm OtherBackend }\nWindow { item(text: vm.title + \"scope->\") }");
    check(quoted.find("std::string(\"scope->\")") != quoted.npos && quoted.find("scope->parent->backend_0.title") != quoted.npos,
          "Borrowing a supplied expression changed a string literal");
    const auto model = compile("import vm Backend\nlet code: View = CodeEditor(model: vm.editor)\nWindow { import vm OtherBackend\ncode(model: vm.editor) }", "LVRS");
    check(model.find("scope->backend_1") != model.npos,"A supplied opaque model lost its stable member reference");
    const auto invalid_model = lvui::compile_native("import vm Backend\nlet code: View = CodeEditor(model: vm.editor)\nWindow { code(model: vm.makeEditor()) }", {.framework="LVRS"});
    check(!invalid_model,"An opaque input accepted an ephemeral call result through a View override");
}
void diagnostics() {
    for (auto source : {
        "var panel: View = Label()\nWindow { panel() }",
        "@Static let panel: View = Label()\nWindow { panel() }",
        "let panel: String = Label()\nWindow { panel() }",
        "let panel: View = 4\nWindow { panel() }",
        "let panel: View = Label()\nWindow { panel(\"override\") }",
        "let panel: View = Label()\nWindow { panel(unknown: 1) }",
        "let panel: View = Label()\nWindow { panel(text: \"one\", text: \"two\") }",
        "import vm Backend\nlet panel: View = Label()\nWindow { VStack { panel(text: vm.title)\nLabel(text: _lvui_supplied_0) } }",
        "import vm Backend\nlet button: View = PushButton()\nWindow { VStack { button(onClick: vm.save)\nPushButton(onClick: _lvui_supplied_0) } }",
        "let panel: View = Label()\nWindow { var panel: Int = 2\nVStack { panel() } }",
        "let loop: View = VStack { loop() }\nWindow { loop() }"})
        check(!lvui::compile_native(source), "Invalid View value was accepted");
    std::string deep = "let v0: View = Label()\n";
    for (int n = 1; n < 70; ++n) deep += "let v" + std::to_string(n) + ": View = VStack { v" + std::to_string(n - 1) + "() }\n";
    auto result = lvui::compile_native(deep + "Window { v69() }");
    check(!result && result.error().message.find("depth") != result.error().message.npos,
          "Unbounded View expansion was accepted");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "syntax";
        if (name == "syntax") syntax(); else if (name == "captured") captured();
        else if (name == "identity") identity(); else if (name == "root") root();
        else if (name == "inputs") inputs_and_slots();
        else if (name == "diagnostics") diagnostics(); else throw std::runtime_error("Unknown View value test");
        std::cout << "LVUI View values " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
