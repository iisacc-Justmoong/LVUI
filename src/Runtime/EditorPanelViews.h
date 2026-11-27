#pragma once

#include <LVUI/Compiler.h>

namespace lvui {

/// AOT-compiles a caller-owned LVUI source with the reusable editor panels and
/// field controls in scope. This function reads no files or global state and
/// returns generated C++ or a diagnostic in the caller's original coordinates.
/// The application supplies the field schema, C++ viewmodel and event targets.
[[nodiscard]] NativeCompileResult compile_editor_panel_views(std::string_view source,
                                                            const NativeCompileOptions& options = {});

} // namespace lvui
