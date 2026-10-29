#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void syntax() {
    auto result = lvui::parse_view("Window { if (vm.ready) { Label(text: \"ready\") } else { Label(text: \"wait\") } }");
    check(result && result->root().children[0].condition && result->root().children[0].children.size() == 2,
          "Conditional branches were not retained");
    auto chained = lvui::parse_view("Window { if (vm.a) { Label() }\nelse if (vm.b) { Label() } else { Label() } }");
    check(chained && chained->root().children[0].children[1].condition, "Else-if chain was flattened or lost");
    auto value = lvui::parse_view("import vm Backend\nlet panel: View = if (vm.ready) { Label() }\nWindow { panel() }");
    check(value && value->bindings()[0].initializer.view->condition, "Conditional visual initializer was not View-valued");
}
void native() {
    auto code = lvui::compile_native("import vm Backend\nWindow { VStack { if (vm.ready && vm.count > 0) { VStack { import child Backend\nLabel(text: child.title) } } else { Label(text: \"Waiting\") }\nLabel(text: vm.title) } }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto token : {"widgets.content_host(", "target.set_content(positive)", "target.set_content(negative)",
                       "native_bindings->bind(", "Conditional owner retired", "weak_activation", "LocalView_0"})
        check(code->find(token) != code->npos, "Conditional AOT lost native activation or ownership");
    check(code->find("parse_view") == code->npos, "Conditional code embeds a source evaluator");
    auto empty = lvui::compile_native("Window { if (false) { Label() } }");
    check(empty && empty->find("target.clear_content()") != empty->npos && empty->find("Lexical parent activation") != empty->npos,
          "Empty conditional lost a valid root or its clear command");
}
void diagnostics() {
    for (auto text : {"Window { if (true) {} }", "Window { if (true) { Label()\nLabel() } }",
                      "Window { if (true { Label() } }", "Window { if (true) { Label() } else {} }",
                      "Window { if (2) { Label() } }", "if (true) { Window { Label() } }"})
        check(!lvui::compile_native(text), "Invalid conditional View accepted");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "syntax";
        if (name == "syntax") syntax(); else if (name == "native") native();
        else if (name == "diagnostics") diagnostics(); else throw std::runtime_error("Unknown conditional test");
        std::cout << "Conditional View " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
