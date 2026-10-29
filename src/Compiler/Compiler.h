#pragma once
#include <LVUI/Document.h>

namespace lvui {
struct NativeCompileOptions final {
    std::string name = "MainView";
    std::vector<std::string> includes;
    std::string framework = "iiGUI";
    StyleTokenOptions style_tokens;
};
// Emits an instantiable C++23 widget class with options/attach/detach. No LVUI runtime parser
// or source document is used by the generated UI. Diagnostics are source based.
using NativeCompileResult = std::expected<std::string, Diagnostic>;
[[nodiscard]] NativeCompileResult compile_native(std::string_view source,
                                               const NativeCompileOptions& = {});
using StyleProfileResult = std::expected<StyleTokenOptions, Diagnostic>;
// Typed let-only .lvui style profile. No runtime parser is emitted.
[[nodiscard]] StyleProfileResult compile_style_profile(std::string_view source,
                                                       const StyleTokenOptions & = {});
[[nodiscard]] NativeCompileResult compile_style_header(const StyleTokenOptions &,
                                                       std::string_view cpp_namespace = "lvui_style");
} // namespace lvui
