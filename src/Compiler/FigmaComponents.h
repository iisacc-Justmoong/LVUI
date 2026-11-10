#pragma once

#include <LVUI/Language.h>
#include <functional>

namespace lvui::detail {
[[nodiscard]] bool is_figma_component(std::string_view) noexcept;
// Canonicalize Figma component names and their authored variants before AOT.
// The callback prevents an unqualified C++ or LVUI binding from being
// mistaken for a literal design variant.
bool normalize_figma_component(View&, const std::function<bool(const Value&)>& is_binding);
}
