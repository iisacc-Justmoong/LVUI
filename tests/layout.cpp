#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string compile(std::string_view source) {
    auto code = lvui::compile_native(source);
    if (!code) throw std::runtime_error(code.error().message);
    return *code;
}
void dynamic() {
    const auto code = compile(R"lvui(import vm Model
Window { VStack(id: "panel", spacing: vm.gap, defaultSpacing: vm.defaultGap, padding: vm.padding,
    leftPadding: vm.left, topPadding: vm.top, rightPadding: vm.right, bottomPadding: vm.bottom,
    width: vm.width, height: vm.height, minimumWidth: vm.minimumWidth, minimumHeight: vm.minimumHeight,
    alignment: vm.alignment, alignmentName: vm.alignmentName, flex: vm.flex) { Label(text: vm.title) } }
)lvui");
    check(code.find("set_stack_layout(") != code.npos && code.find("NativeWidgetLayout.h") != code.npos,
          "A dynamic stack layout was not connected to one native layout update");
    for (auto field : {"gap", "defaultGap", "padding", "left", "top", "right", "bottom", "width", "height", "minimumWidth", "minimumHeight", "alignment", "alignmentName", "flex"})
        check(code.find(std::string("backend_0.") + field) != code.npos, "A native layout input lost its source");
}
void alignment() {
    for (auto value : {"center", "leading", "trailing", "stretch", "top", "bottom", "topLeading", "topTrailing", "bottomLeading", "bottomTrailing"})
        for (auto stack : {"HStack", "VStack", "ZStack"})
            check(!compile(std::string("Window { ") + stack + "(alignment: ." + value + ") { Label() } }").empty(),
                  "A named stack axis alignment was rejected");
    check(!compile("import vm Model\nWindow { ZStack(alignment: vm.alignment, alignmentName: vm.name) { Label() } }").empty(),
          "Native stack alignment values cannot be bound");
    check(!compile("import vm Model\nWindow { HStack(width: vm.width, alignment: \"TOP\") {} }").empty() &&
          !compile("Window { HStack(alignment: \"TOP\") {} }").empty(),"Static and dynamic layouts disagreed on named alignment normalization");
}
void constants() {
    const auto source = "Window { VStack(spacing: -1, defaultSpacing: 6, padding: 4, leftPadding: 2, width: 200, height: 100, minimumWidth: 120, minimumHeight: 80) { Label() } }";
    const auto code = compile(source);
    check(code == compile(source), "Stack layout generation was not deterministic");
    check(code.find("set_stack_layout(") != code.npos, "Constant stack dimensions bypassed the native layout contract");
    check(code.find("LVUI/")==code.npos && code.find(".spacing = 6")!=code.npos && code.find(".extent = {200, 100}")!=code.npos,
          "A constant layout retained runtime normalization or lost AOT folding");
}
void diagnostics() {
    for (auto source : {
        "Window { VStack(width: -1) {} }", "Window { VStack(height: 8193) {} }",
        "Window { HStack(padding: -1) {} }", "Window { HStack(leftPadding: -1) {} }",
        "Window { ZStack(topPadding: 8193) {} }", "Window { ZStack(defaultSpacing: -1) {} }",
        "Window { VStack(padding: 8193, leftPadding: 0) {} }",
        "Window { VStack(minimumWidth: -1) {} }", "Window { VStack(minimumHeight: 8193) {} }",
        "Window { VStack(flex: -1) {} }", "Window { HStack(alignment: .unknown) {} }",
        "Window { VStack(alignment: \"unknown\") {} }", "Window { HStack(spacing: true) {} }",
        "var width: String = \"200\"\nWindow { VStack(width: width) {} }",
        "var flex: Bool = true\nWindow { VStack(flex: flex) {} }",
        "var flag: Int = 1\nWindow { VStack(intrinsicWidth: flag) {} }",
        "var name: Int = 1\nWindow { VStack(alignmentName: name) {} }",
        "var align: Int = 1\nWindow { VStack(alignment: align) {} }"}) {
        const auto code = lvui::compile_native(source);
        check(!code && code.error().line > 0 && code.error().column > 0,
              "An invalid constant layout did not produce a source diagnostic");
    }
}
int main(int argc, char** argv) {
    try {
        std::string which = argc > 1 ? argv[1] : "dynamic";
        if (which == "dynamic") dynamic(); else if (which == "alignment") alignment();
        else if (which == "constants") constants(); else if (which == "diagnostics") diagnostics();
        else throw std::runtime_error("Unknown layout test");
        std::cout << "Native layout compiler " << which << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
