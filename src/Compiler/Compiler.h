#pragma once
#include <LVUI/Document.h>

namespace lvui {
struct NativeCompileOptions final {
    std::string name = "MainView";
    std::vector<std::string> includes;
    std::string framework = "iiGUI";
    StyleTokenOptions style_tokens;
    // Generate a mount-independent widget factory for an externally owned surface.
    bool view_factory_only = false;
};
// Emits an instantiable C++23 widget class with options/attach/detach. No LVUI runtime parser
// or source document is used by the generated UI. Diagnostics are source based.
using NativeCompileResult = std::expected<std::string, Diagnostic>;
[[nodiscard]] NativeCompileResult compile_native(std::string_view source,
                                               const NativeCompileOptions& = {});
using StyleProfileResult = std::expected<StyleTokenOptions, Diagnostic>;
// Typed immutable .lvui style profile (const; legacy let is accepted). No runtime parser is emitted.
[[nodiscard]] StyleProfileResult compile_style_profile(std::string_view source,
                                                       const StyleTokenOptions & = {});
[[nodiscard]] NativeCompileResult compile_style_header(const StyleTokenOptions &,
                                                       std::string_view cpp_namespace = "lvui_style");
} // namespace lvui
