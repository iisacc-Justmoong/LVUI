#include <LVUI/Compiler.h>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    lvui::NativeCompileOptions options;
    options.name = "NativeTypographyPolicy";
    options.style_tokens.overrides["--lvrs-theme-typography-body-text-body"] = 19.5;
    options.style_tokens.overrides["--lvrs-theme-typography-weight-medium"] = 700.;
    auto result = lvui::compile_native(
        "Window { Label(id: \"generated\", text: \"Generated typography\", style: .body) }", options);
    if (!result) { std::cerr << result.error().message << '\n'; return 1; }
    std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
    output << *result;
    return output ? 0 : 1;
}
