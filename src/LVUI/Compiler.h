#pragma once
#include "Document.h"

namespace lvui {
struct NativeCompileOptions final {
    std::string name = "MainView";
    std::vector<std::string> includes;
    std::string framework = "iiGUI"; // LVRS selects the independently packaged native framework.
};
// Emits an instantiable C++23 widget class with options/attach/detach. No LVUI runtime parser
// or source document is used by the generated UI. Diagnostics are source based.
using NativeCompileResult = std::expected<std::string, Diagnostic>;
[[nodiscard]] NativeCompileResult compile_native(std::string_view source,
                                               const NativeCompileOptions& = {});
} // namespace lvui
