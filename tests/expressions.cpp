#include <LVUI/Compiler.h>
#include <LVUI/ConstantExpression.h>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
lvui::ViewDocument parsed(std::string_view source) {
    auto result = lvui::parse_view(source);
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
lvui::Value expression(std::string_view source) {
    return parsed("Label(text: " + std::string(source) + ")").root().inputs.at(0).value;
}
lvui::Value constant(std::string_view source) {
    auto result = lvui::fold_constant_expression(expression(source), {});
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void invalid(std::string_view source, std::string_view message) {
    auto result = lvui::parse_view(source);
    check(!result && result.error().offset <= source.size(), "Invalid source accepted or diagnostic out of bounds");
    check(result.error().message.find(message) != std::string::npos, "Unexpected syntax diagnostic");
}
void invalid_constant(std::string_view source, std::string_view message) {
    auto result = lvui::fold_constant_expression(expression(source), {});
    check(!result, "Invalid constant expression accepted");
    if (result.error().message.find(message) == std::string::npos)
        throw std::runtime_error("Unexpected constant diagnostic: " + result.error().message);
}
void precedence() {
    const auto value = expression("1 + 2 * 3 - 8 / 4");
    check(value.kind == lvui::ValueKind::binary && value.text == "-", "Additive associativity lost");
    check(value.elements[0].text == "+" && value.elements[0].elements[1].text == "*", "Multiplication precedence lost");
    check(constant("1 + 2 * 3 - 8 / 4").text == "5", "Arithmetic precedence evaluated incorrectly");
    check(constant("(1 + 2) * 3").text == "9", "Grouping was ignored");
    check(constant("10 - 3 - 2").text == "5", "Subtraction became right associative");
    check(constant("!false && 1 < 2 || false").text == "true", "Logical precedence lost");
    check(constant("-5 % 2").text == "-1", "Negative remainder contract changed");
    check(expression("-4").kind == lvui::ValueKind::number, "Negative literal compatibility lost");
    check(expression("-vm.count").kind == lvui::ValueKind::unary, "Unary property expression not parsed");
}
void conditionals() {
    const auto value = expression("true ? 1 : false ? 2 : 3");
    check(value.kind == lvui::ValueKind::conditional && value.elements[2].kind == lvui::ValueKind::conditional,
          "Conditional associativity lost");
    check(constant("true ? 7 : 1 / 0").text == "7", "Inactive branch was evaluated");
    check(constant("false && (1 / 0 > 0)").text == "false", "And did not short circuit");
    check(constant("true || (1 / 0 > 0)").text == "true", "Or did not short circuit");
    check(constant("false ? \"left\" : \"right\" + \"!\"").text == "right!", "String conditional lost its value");
    invalid_constant("true ? 1 : \"text\"", "compatible");
    invalid_constant("false && 4", "Bool");
    invalid_constant("1 ? 2 : 3", "Bool");
}
void postfix() {
    const auto value = expression("vm.rows[vm.index + 1].name");
    check(value.kind == lvui::ValueKind::member && value.text == "name", "Member after index lost");
    check(value.elements[0].kind == lvui::ValueKind::index && value.elements[0].elements[1].text == "+", "Typed index expression lost");
    check(expression("vm.rows[0].open()").kind == lvui::ValueKind::invocation, "Postfix invocation became a reference");
    check(expression("vm.open").kind == lvui::ValueKind::reference, "Method reference became eager");
    check(expression("vm.open(1 + 2)").kind == lvui::ValueKind::call, "Explicit call became a reference");
    check(constant("[4, 5 + 1][1]").text == "6", "Constant list index not folded");
    check(constant("[[1, 2], [3, 4]][1][0]").text == "3", "Nested constant list index lost");
    invalid_constant("[1, 2][-1]", "bounds");
    invalid_constant("[1, 2][2]", "bounds");
    invalid_constant("[1, 2][0.5]", "bounds");
}
void annotations() {
    const auto document = parsed("@Static let initial: Int = 2 + 3\n@Observable var count: Int = 0\nWindow { VStack { @Observable let title = \"Ready\"\nLabel(text: title) } }");
    check(document.bindings()[0].annotation == lvui::BindingAnnotation::static_value, "Static annotation lost");
    check(document.bindings()[1].annotation == lvui::BindingAnnotation::observable, "Observable annotation lost");
    check(document.root().children[0].bindings[0].annotation == lvui::BindingAnnotation::observable, "Nested annotation lost");
    invalid("@Static var n = 1\nLabel()", "immutable");
    invalid("@Unknown let n = 1\nLabel()", "Unknown declaration");
    invalid("@Static @Observable let n = 1\nLabel()", "conflicting");
    invalid("@Observable @Observable let n = 1\nLabel()", "conflicting");
    invalid("@Static import vm Backend\nLabel()", "let or var");
}
void diagnostics() {
    invalid("Label(text: 1 +)", "value");
    invalid("Label(text: true ? 1)", "conditional");
    invalid("Label(text: 1 === 1)", "JavaScript");
    invalid("Label(text: 1 & 2)", "character");
    invalid_constant("1 / 0", "zero");
    invalid_constant("1 % 0", "zero");
    invalid_constant("1e308 * 1e308", "non-finite");
    invalid_constant("true + 1", "numeric");
    invalid_constant("1 == \"1\"", "compatible");
    const std::string source = "Label(text: (\n1 +\n2 * 3))";
    check(parsed(source).root().inputs[0].value.position.line == 2, "Operator source position lost");
    check(constant("1 +\n2").text == "3", "Continuation after operator rejected");
}
void limits() {
    std::string source = "1";
    for (int i = 0; i < 63; ++i) source += " + 1";
    (void)expression(source);
    invalid("Label(text: " + source + " + 1)", "depth");
    invalid("Label(text: " + std::string(65, '!') + "true)", "depth");
    const std::string fragment = "Window { Label(text: true ? \"yes\" : \"no\") }";
    for (std::size_t i = 0; i < fragment.size(); ++i) {
        auto result = lvui::parse_view(std::string_view(fragment).substr(0, i));
        check(!result && result.error().offset <= i, "Truncated expression accepted or diagnostic escaped source");
    }
}
void aot() {
    auto result = lvui::compile_native("@Static let gap: Metric = .space4 + 2\nlet heading: String = true ? \"Dense \" + \"tools\" : \"Other\"\nWindow(width: 640 + 80) { VStack(spacing: gap) { Label(text: heading)\nPushButton(text: \"Run\", enabled: 3 * 4 >= 12) } }", {.framework = "LVRS"});
    if (!result) throw std::runtime_error(result.error().message);
    check(result->find("Dense tools") != result->npos && result->find(".size = {720, 420}") != result->npos,
          "Native AOT did not emit folded construction values");
    check(result->find("fold_constant_expression") == result->npos && result->find("parse_view") == result->npos,
          "Compiler evaluator leaked into native UI output");
    auto snapshot = lvui::compile_native("import vm Backend\n@Static let title = vm.title\nWindow { Label(text: title) }");
    check(snapshot && snapshot->find("Mode::fixed") != snapshot->npos && snapshot->find("scope->inferred_title = Scope::slot_title::make(")!=snapshot->npos,
          "Foreign static snapshot did not use one activation-owned copy");
    auto native = lvui::compile_native("import vm Backend\nlet count: Int = vm.count * 2\nWindow { VStack { Label(text: vm.ready ? vm.title + \"!\" : \"Waiting\")\nLabel(text: count)\nPushButton(enabled: count > 0) } }");
    if (!native) throw std::runtime_error(native.error().message);
    check(native->find("native_bindings->derive(") != native->npos && native->find("native_bindings->bind(") != native->npos &&
          native->find("lvui::native::choose(") != native->npos && native->find("lvui::native::multiply(") != native->npos,
          "Native reactive formulas were not compiled to typed C++ getters");
    check(!lvui::compile_native("Window { Label(enabled: 1 + true) }"), "Invalid typed expression accepted by AOT");
    auto members = lvui::compile_native("import vm Backend\nlet details: String = vm.document.title\nWindow { Label(text: details) }");
    check(members && members->find("lvui::native::named_member(") != members->npos && members->find("object.document") != members->npos &&
          members->find("object.title") != members->npos, "Typed C++ member paths were not generated");
    auto alias = lvui::compile_native("import vm Backend\nlet model = vm.document\nWindow { CodeEditor(model: model) }", {.framework="LVRS"});
    check(alias && alias->find("value.model = lvui::native::initial((*scope->inferred_model))") != alias->npos,
          "An opaque native model input rejected a scoped value alias");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "precedence";
        if (name == "precedence") precedence(); else if (name == "conditionals") conditionals();
        else if (name == "postfix") postfix(); else if (name == "annotations") annotations();
        else if (name == "diagnostics") diagnostics(); else if (name == "limits") limits();
        else if (name == "aot") aot(); else throw std::runtime_error("Unknown expression test");
        std::cout << "LVUI expressions " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
