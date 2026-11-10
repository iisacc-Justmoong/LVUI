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
    auto local = lvui::parse_view("Window { if (vm.ready) { import child Backend\nlet caption: String = child.title\nLabel(text: caption) } else { let caption: String = \"wait\"\nLabel(text: caption) } }");
    check(local && local->root().children[0].children[0].imports.size()==1 &&
          local->root().children[0].children[0].bindings.size()==1 &&
          local->root().children[0].children[1].bindings.size()==1,
          "Branch-local declarations were not retained in their separate scopes");
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
    auto branch = lvui::compile_native("import vm Backend\nWindow { if (vm.ready) { import child Backend\nlet caption: String = child.title\nLabel(text: caption) } else { let caption: String = \"wait\"\nLabel(text: caption) } }");
    if(!branch)throw std::runtime_error(branch.error().message);
    auto menu = lvui::compile_native("import vm Backend\nWindow { ContextMenu { if (vm.ready) { import child Backend\nlet caption: String = child.title\nlet Action: View = ContextMenuItem(label: caption, hasChildItems: false)\nAction() } else { ContextMenuDivider() } } }");
    if(!menu)throw std::runtime_error(menu.error().message);
    auto chained = lvui::compile_native("import vm Backend\nWindow { if (vm.a) { let caption: String = \"A\"\nLabel(text: caption) } else if (vm.b) { import child Backend\nLabel(text: child.title) } else { let caption: String = \"C\"\nLabel(text: caption) } }");
    if(!chained)throw std::runtime_error(chained.error().message);
    auto typed = lvui::compile_native("import vm Backend\nWindow { ContextMenu { if (vm.ready) { import class Entry EntryModel\nlet Action(item: Entry): View = ContextMenuItem(label: item.title, hasChildItems: false)\nAction(item: vm.entry) } } }");
    if(!typed)throw std::runtime_error(typed.error().message);
}
void diagnostics() {
    for (auto text : {"Window { if (true) {} }", "Window { if (true) { Label()\nLabel() } }",
                      "Window { if (true { Label() } }", "Window { if (true) { Label() } else {} }",
                      "Window { if (2) { Label() } }", "if (true) { Window { Label() } }",
                      "Window { if (true) { let x: String = \"one\"\nlet x: String = \"two\"\nLabel() } }",
                      "Window { if (true) { let x: String = \"one\" } }",
                      "Window { if (true) { let x: String = \"one\"\nLabel()\nLabel() } }",
                      "Window { VStack { if (true) { let caption: String = \"local\"\nLabel(text: caption) }\nLabel(text: caption) } }",
                      "Window { if (true) { import child Backend\nLabel() } else { Label(text: child.title) } }",
                      "Window { if (child.ready) { import child Backend\nLabel() } }"})
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
