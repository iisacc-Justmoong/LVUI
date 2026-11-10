#include <LVUI/Language.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
lvui::ViewDocument good(std::string_view source) {
    auto result = lvui::parse_view(source);
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void bad(std::string_view source, std::string_view reason) {
    auto result = lvui::parse_view(source);
    check(!result && result.error().offset <= source.size(), "Invalid component source accepted or unbounded diagnostic");
    if (result.error().message.find(reason) == std::string::npos)
        throw std::runtime_error("Unexpected diagnostic: " + result.error().message);
}
void catalog() {
    auto entries = lvui::keywords();
    check(lvui::find_keyword("const") && lvui::find_keyword("const")->kind == lvui::KeywordKind::declaration,
          "const is not an LVUI declaration keyword");
    check(entries.size() > 100, "LVUI component keywords missing from the catalog");
    std::string_view previous;
    unsigned views = 0, modifiers = 0;
    for (const auto& word : entries) {
        check(previous.empty() || previous < word.spelling, "Keyword catalog is not unique and sorted");
        previous = word.spelling;
        check(lvui::find_keyword(word.spelling) == &word && lvui::is_reserved_identifier(word.spelling), "Keyword lookup mismatch");
        if (word.kind == lvui::KeywordKind::view) {
            ++views;
            check(word.source_file == "catalog/lvui-keywords.json" && word.source_line > 0, "Visual keyword lacks its LVUI registry location");
            auto parsed = good(std::string(word.spelling) + "()");
            check(parsed.root().component == &word, "Parser does not consume the builtin keyword table");
        }
        modifiers += word.kind == lvui::KeywordKind::modifier;
    }
    check(views > 100 && modifiers == 3, "Visual keyword classification incomplete");
    for (auto name : {"Avatar", "Tab", "CardImageContent", "AdaptiveLayoutHost", "ColorPickerSurface", "WindowNoiseTexture", "QuantitySlot"})
        check(lvui::find_keyword(name)->kind == lvui::KeywordKind::view, "Missing LVUI visual keyword");
    check(lvui::find_keyword("NativeView")&&lvui::find_keyword("NativeView")->kind==lvui::KeywordKind::view,"Native C++ visual source is not a View constructor");
    for (auto name : {"Theme", "Motion", "Navigator", "ViewModel", "ColorPickerModel", "Backend"})
        check(!lvui::is_reserved_identifier(name), "Nonvisual service became a visual keyword");
    check(!lvui::find_keyword("pushButton") && !lvui::find_keyword("PushButtonSuffix"), "Keyword matching is not exact/case-sensitive");
}
void lexing() {
    auto fractions=lvui::tokenize(".25 .5e-1 Color.primary .primary");
    check(fractions&&fractions->at(0).kind==lvui::TokenKind::number&&fractions->at(1).kind==lvui::TokenKind::number&&fractions->at(3).kind==lvui::TokenKind::dot,
          "Leading decimal literals consumed a style selector or failed to tokenize");
    auto tokens = lvui::tokenize("// Label ignored\nLabel(text: \"PushButton\") /* Avatar ignored */\nPushButton(vm.save)");
    check(bool(tokens), "Valid component source did not tokenize");
    unsigned reserved = 0, strings = 0;
    for (const auto& token : *tokens) {
        reserved += token.kind == lvui::TokenKind::keyword;
        if (token.kind == lvui::TokenKind::string) {
            ++strings; check(token.text == "PushButton" && !token.reserved, "String content treated as a keyword");
        }
    }
    check(reserved == 2 && strings == 1, "Comments/string boundaries are not lexical boundaries");
    auto annotated = lvui::tokenize("Label(\"한글\", @)");
    check(bool(annotated) && std::ranges::any_of(*annotated, [](const auto& token) {
        return token.kind == lvui::TokenKind::annotation && token.column == 13;
    }), "Annotation token lost its UTF-8 scalar position");
    auto utf8 = lvui::tokenize("Label(\"한글\", $)");
    check(!utf8 && utf8.error().column == 13, "UTF-8 scalar diagnostic positions incorrect");
    auto nul = lvui::tokenize(std::string("Label(\"한글\", ") + '\0' + ")");
    check(!nul && nul.error().column == 13, "NUL diagnostic does not point to the offending scalar position");
    check(!lvui::tokenize("\"unterminated") && !lvui::tokenize("/* unterminated"), "Truncated lexical token accepted");
}
void views() {
    const auto constants = good("const title: String = \"Counter\"\nvar count: Int = 0\nWindow { @State @Bindable var local: Int = 1\n Label(text: title) }");
    check(constants.bindings().size() == 2 && !constants.bindings()[0].mutable_value && constants.bindings()[1].mutable_value,
          "const and var lost their mutability distinction");
    check(constants.root().bindings.size() == 1 && constants.root().bindings[0].mutable_value &&
          constants.root().bindings[0].explicit_state && constants.root().bindings[0].explicit_bindable,
          "Explicit state and binding markers were not preserved");
    std::string source = R"(import vm CounterBackend
let title: String = "Counter"
var value: Int = 6
ApplicationWindow(title: title) {
    VStack(spacing: 16) {
        Label(text: "한글\nHello")
        PushButton(vm.increment) {
            text: "+"
            enabled: true
        }
        Tab(iconName: "home", text: "Home")
        ColorPickerSurface(field: .Wheel)
        List(items: [value, 2, vm.reset()])
    }
})";
    auto document = good(source);
    source.assign(source.size(), 'x');
    check(document.imports().size() == 1 && document.imports()[0].name == "vm", "Backend import not retained");
    check(document.bindings().size() == 2 && document.bindings()[1].mutable_value, "Scalar declarations not retained");
    const auto& stack = document.root().children.at(0);
    check(stack.component->spelling == "VStack" && stack.children.size() == 5, "Brace child View arguments lost");
    check(stack.children[0].inputs[0].value.text == "한글\nHello", "Strings depend on the source lifetime or escaping failed");
    check(stack.children[1].inputs[0].value.kind == lvui::ValueKind::reference, "Method reference became a call");
    check(stack.children[1].inputs[1].name == "text", "Body properties not retained");
    check(stack.children[4].inputs[0].value.elements[2].kind == lvui::ValueKind::call, "Explicit method call became a reference");
    check(good("VStack { Label()\n HStack { Avatar()\n Spacer() } }").root().children.size() == 2, "Optional empty parentheses/nested braces failed");
    check(good("VStack { Label(text: \"A\") }").root().children.size() == 1,
          "Omitting empty constructor parentheses or an empty child body was rejected");
    check(good("Label").root().component->spelling == "Label",
          "Omitting both empty View sections was rejected at the root");
    const auto bare_children = good("VStack { Label, HStack { Spacer } }");
    check(bare_children.root().children.size() == 2 &&
          bare_children.root().children[0].component->spelling == "Label" &&
          bare_children.root().children[1].children[0].component->spelling == "Spacer",
          "Nested Views with both empty sections omitted lost their order or structure");
    check(good("let Content: View = Label\nWindow { Content }").root().children[0].custom_component == "Content",
          "A View value or reference without empty sections was rejected");
    const auto view_argument = good("HStack() { VStack() }");
    check(view_argument.root().component->spelling == "HStack" &&
          view_argument.root().children.size() == 1 &&
          view_argument.root().children.front().component->spelling == "VStack",
          "A View passed as a body argument lost its type or parent");
    const auto view_arguments = good("HStack() { VStack(), ZStack(), Label(text: \"Item\") }");
    check(view_arguments.root().children.size() == 3 &&
          view_arguments.root().children[0].component->spelling == "VStack" &&
          view_arguments.root().children[1].component->spelling == "ZStack" &&
          view_arguments.root().children[2].component->spelling == "Label",
          "View body arguments lost their types or declaration order");
    bad("HStack() { VStack() }}", "root");
    const auto nested_stacks = good("Window { ZStack { VStack { Label() }\n HStack { Label() } } }");
    const auto& overlay = nested_stacks.root().children.at(0);
    check(overlay.component->spelling == "ZStack" && overlay.children.at(0).component->spelling == "VStack" &&
          overlay.children.at(1).component->spelling == "HStack", "The three explicit stacks did not retain their nesting order");
    bad("Window { div { Label() } }", "HStack");
    check(good("let div: View = HStack {}\nWindow { div {} }").root().children.at(0).custom_component == "div",
          "An explicitly declared custom View was mistaken for a built-in div");
    const auto markup = lvui::parse_native_markup("<Window><ZStack><VStack><Label/></VStack><HStack><Label/></HStack></ZStack></Window>");
    check(markup && markup->root().children.at(0).component->spelling == "ZStack" &&
          markup->root().children.at(0).children.at(0).component->spelling == "VStack" &&
          markup->root().children.at(0).children.at(1).component->spelling == "HStack",
          "Legacy markup did not preserve explicit stack types");
    for (auto source : {"<Window><div/></Window>", "<Window><HStack style=\"flex-direction: row\"/></Window>"}) {
        const auto result = lvui::parse_native_markup(source);
        check(!result && result.error().message.find("HStack") != std::string::npos,
              "Implicit div or flex-direction layout did not suggest explicit stacks");
    }
    const auto immediate = good("HStack(id:xxx){gap:Metric.sideBar, IconButton(label: RandomString(), icon: random){onClick: vm.method()}}");
    check(immediate.root().inputs.size() == 2 && immediate.root().inputs[0].value.text == "xxx" &&
          immediate.root().inputs[1].value.text == "Metric.sideBar" && immediate.root().children.size() == 1,
          "Immediate declaration lost its identifier, body input, or child");
    const auto& button = immediate.root().children.front();
    check(button.inputs.size() == 3 && button.inputs[0].value.kind == lvui::ValueKind::call &&
          button.inputs[2].name == "onClick" && button.inputs[2].value.text == "vm.method" &&
          button.inputs[2].value.kind == lvui::ValueKind::call,
          "Immediate declaration lost value or event calls");
    check(good("VStack { Label(),\n Avatar(), }").root().children.size() == 2,
          "Comma and newline View separators did not compose");
    bad("VStack { Label(),, Avatar() }", "View");
    const auto script = good(R"(import vm PanelModel
@State var expanded: Bool = false
Window { PushButton(onClick: {
    expanded = true
    vm.didOpen(expanded)
}) })");
    const auto& action = script.root().children.at(0).inputs.at(0).value;
    check(action.kind == lvui::ValueKind::action && action.elements.size() == 2 &&
          action.elements[0].kind == lvui::ValueKind::assignment &&
          action.elements[1].kind == lvui::ValueKind::call && action.elements[1].text == "vm.didOpen",
          "Tagless event script lost its assignment or native call");
    const auto flow = good(R"(import vm PanelModel
Window { PushButton(onClick: {
    if (vm.ready) {
        for (item in vm.items) {
            if (item.skip) { continue } else if (item.stop) { break }
            vm.open(item.id)
        }
    } else { return }
}) })");
    check(flow.root().children.at(0).inputs.at(0).value.elements.size() == 1,
          "Script condition or nested loop was not parsed as one statement");
    bad("Window { PushButton(onClick: { if (true) { } else }) }", "{");
    bad("Window { PushButton(onClick: { for (item vm.items) { return } }) }", "in");
    bad("Window { PushButton(onClick: { for (if in [1]) { return } }) }", "reserved");
    (void)good("Window { PushButton(onClick: { if (true) {} else {} }) }");
    auto types = good("import class Domain.Model\nimport vm Domain.Model\nLabel()");
    check(types.imports().size() == 2 && types.imports()[0].type_only && types.imports()[0].name == "Model" &&
          !types.imports()[1].type_only, "Type blueprint import created an instance declaration");
    bad("import class Domain.Model\nlet Model = 1\nLabel()", "Duplicate");
    const auto aliases=good("import class Row App.HierarchyItem\nimport class AppWindow App.Window\nimport state App.Color\nLabel()");
    check(aliases.imports()[0].name=="Row"&&aliases.imports()[0].type=="App.HierarchyItem"&&aliases.imports()[0].type_only&&aliases.imports()[1].name=="AppWindow"&&aliases.imports()[2].type=="App.Color"&&!aliases.imports()[2].type_only,"A C++ type alias lost its blueprint-only contract or native type name");
    bad("import class Bad.Alias App.Window\nLabel()","unqualified");
    bad("import class Row App.import\nLabel()","reserved");
}
void shadowing() {
    const auto enumeration=good("Card(type: .Link)");
    check(enumeration.root().inputs[0].value.kind==lvui::ValueKind::enumeration&&enumeration.root().inputs[0].value.text==".Link","Component keyword cannot be used as an enum value");
    bad("Card(type: .import)","reserved");
    for (const auto& word : lvui::keywords()) {
        auto name = std::string(word.spelling);
        bad("let " + name + " = 1\nLabel()", "reserved");
        bad("var " + name + " = 1\nLabel()", "reserved");
        bad("import " + name + " Backend\nLabel()", "reserved");
    }
    (void)good("let label = \"Label\"\nimport custom Backend\nLabel(text: label)");
    check(good("import vm PushButton\nLabel()").imports()[0].type=="PushButton","A C++ type name was confused with the declaration's binding name");
    bad("import class App.HierarchyItem\nLabel()","alias");
    bad("let name = 1\nvar name = 2\nLabel()", "Duplicate");
}
void diagnostics() {
    bad("UnknownWidget()", "Unknown"); bad("View()", "concrete View");
    bad("SpringBehavior()", "concrete View");
    bad("Label(text: \"a\", text: \"b\")", "Duplicate");
    bad("Label(text: \"a\") { text: \"b\" }", "Duplicate");
    bad("VStack { 6 }", "View"); bad("Label(text:)", "value");
    bad("Label(text: \"a\");", "character");
    bad("Label()\nAvatar()", "root");
    bad("Label(text: 1e999)", "number"); bad("Label(text: [1,)", "value");
    bad("Label(text: \"bad\\q\")", "escape");
    bad("let value: Object = { count: 1 }\nLabel()", "C++");
    bad("Window { var value = { count: 1 }\n Label() }", "C++");
    bad("Window { Label(text: { text: \"hello\" }) }", "C++");
    bad("let value: Object = {}\nLabel()", "C++");
    bad("let values = [{ count: 1 }]\nLabel()", "C++");
    bad("import vm Backend\nlet run: () -> Void = { vm.run() }\nWindow {}", "C++");
    bad("import vm Backend\nWindow { var run = { vm.run() }\nLabel() }", "C++");
    bad("import vm Backend\nvar run: () -> Void = vm.run\nWindow { PushButton(onClick: { run = { vm.other() } }) }", "C++");
    bad("let run: () -> Void = {}\nWindow {}", "C++");
    bad("import vm Backend\nlet run = true ? { vm.run() } : vm.run\nWindow {}", "C++");
    bad("import vm Backend\nlet Control(action: () -> Void = { vm.run() }): View = PushButton(onClick: action)\nWindow {}", "C++");
    bad("class Model {}\nWindow {}", "Unknown");
    bad("object model {}\nWindow {}", "Unknown");
    bad("func calculate() {}\nWindow {}", "Unknown");
    bad("let calculate(value: Int): Int = value\nWindow {}", "View");
    (void)good("import vm Backend\nlet run: () -> Void = vm.run\nWindow { var count: Int = 0\nPushButton(onClick: { count = count + 1\nrun() }) }");
    (void)good("import vm Backend\nlet Control: View = PushButton(onClick: { vm.run() })\nWindow { Control() }");
    (void)good("Window { PushButton(onClick: {}) }");
    auto invalid = std::string("Label(\"") + char(0xff) + "\")";
    bad(invalid, "UTF-8"); bad(std::string("Label()\0", 8), "NUL");
    const std::string sample = "VStack { Label(text: \"한글\")\n Avatar() }";
    for (std::size_t i = 0; i < sample.size(); ++i) {
        const auto prefix = std::string_view(sample).substr(0, i);
        auto result = lvui::parse_view(prefix);
        // With empty sections omitted, these two prefixes are complete Views.
        if (prefix == "VStack" || prefix == "VStack ") {
            check(result && result->root().component->spelling == "VStack" && result->root().children.empty(),
                  "A complete empty View was rejected as truncated input");
            continue;
        }
        check(!result && result.error().offset <= i, "Truncation accepted or diagnostic outside source");
    }
}
void limits() {
    bad(std::string(1024 * 1024 + 1, ' '), "1 MiB");
    std::string source;
    for (unsigned i = 0; i < 64; ++i) source += "VStack {\n";
    source.append(64, '}'); (void)good(source);
    bad("VStack {" + source + "}", "depth");
    source = "VStack {\n";
    for (unsigned i = 0; i < 4095; ++i) source += "Label()\n";
    (void)good(source + "}"); bad(source + "Label()\n}", "4096");
    check(!lvui::tokenize(std::string(70000, '(')), "Token count not bounded");
}
}
int main(int argc, char** argv) {
    try {
        const std::string test = argc > 1 ? argv[1] : "catalog";
        if (test == "catalog") catalog(); else if (test == "lexing") lexing();
        else if (test == "views") views(); else if (test == "shadowing") shadowing();
        else if (test == "diagnostics") diagnostics(); else if (test == "limits") limits();
        else throw std::runtime_error("Unknown case");
        std::cout << "LVUI language " << test << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
