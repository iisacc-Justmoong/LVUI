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
    check(entries.size() > 100, "LVRS inline/native components missing from the catalog");
    std::string_view previous;
    unsigned views = 0, modifiers = 0;
    for (const auto& word : entries) {
        check(previous.empty() || previous < word.spelling, "Keyword catalog is not unique and sorted");
        previous = word.spelling;
        check(lvui::find_keyword(word.spelling) == &word && lvui::is_reserved_identifier(word.spelling), "Keyword lookup mismatch");
        if (word.kind == lvui::KeywordKind::view) {
            ++views;
            check(!word.source_file.empty() && word.source_line > 0, "Visual keyword lacks source evidence");
            auto parsed = good(std::string(word.spelling) + "()");
            check(parsed.root().component == &word, "Parser does not consume the builtin keyword table");
        }
        modifiers += word.kind == lvui::KeywordKind::modifier;
    }
    check(views > 100 && modifiers == 3, "Visual source classification incomplete");
    for (auto name : {"Avatar", "Tab", "CardImageContent", "AdaptiveLayoutHost", "ColorPickerSurface", "WindowNoiseTexture", "QuantitySlot"})
        check(lvui::find_keyword(name)->kind == lvui::KeywordKind::view, "Missing QML/inline/native visual keyword");
    for (auto name : {"Theme", "Motion", "Navigator", "ViewModel", "ColorPickerModel", "Backend"})
        check(!lvui::is_reserved_identifier(name), "Nonvisual service became a visual keyword");
    check(!lvui::find_keyword("pushButton") && !lvui::find_keyword("PushButtonSuffix"), "Keyword matching is not exact/case-sensitive");
}
void lexing() {
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
    auto utf8 = lvui::tokenize("Label(\"한글\", @)");
    check(!utf8 && utf8.error().column == 13, "UTF-8 scalar diagnostic positions incorrect");
    auto nul = lvui::tokenize(std::string("Label(\"한글\", ") + '\0' + ")");
    check(!nul && nul.error().column == 13, "NUL diagnostic does not point to the offending scalar position");
    check(!lvui::tokenize("\"unterminated") && !lvui::tokenize("/* unterminated"), "Truncated lexical token accepted");
}
void views() {
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
    bad("import vm PushButton\nLabel()", "reserved");
    bad("let name = 1\nvar name = 2\nLabel()", "Duplicate");
}
void diagnostics() {
    bad("UnknownWidget()", "Unknown"); bad("View()", "concrete View");
    bad("SpringBehavior()", "concrete View");
    bad("Label(text: \"a\", text: \"b\")", "Duplicate");
    bad("Label(text: \"a\") { text: \"b\" }", "Duplicate");
    bad("VStack { 6 }", "View"); bad("Label(text:)", "value");
    bad("Label(text: \"a\");", "character");
    bad("Label()\nAvatar()", "root"); bad("Label", "Expected");
    bad("Label(text: 1e999)", "number"); bad("Label(text: [1,)", "value");
    bad("Label(text: \"bad\\q\")", "escape");
    auto invalid = std::string("Label(\"") + char(0xff) + "\")";
    bad(invalid, "UTF-8"); bad(std::string("Label()\0", 8), "NUL");
    const std::string sample = "VStack { Label(text: \"한글\")\n Avatar() }";
    for (std::size_t i = 0; i < sample.size(); ++i) {
        auto result = lvui::parse_view(std::string_view(sample).substr(0, i));
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
