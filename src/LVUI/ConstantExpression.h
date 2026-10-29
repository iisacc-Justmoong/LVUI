#pragma once
#include "Language.h"
#include <functional>

namespace lvui {
using ConstantExpressionResult = std::expected<Value, Diagnostic>;
using ConstantResolver = std::function<ConstantExpressionResult(const Value&)>;
// Compiler-only constant folding. It never invokes imported C++ objects and is
// not referenced by the emitted native widget code. Foreign values are resolved
// by the compiler's lexical/style environment, never by source-string evaluation.
[[nodiscard]] ConstantExpressionResult fold_constant_expression(
    const Value&, const ConstantResolver&, SourcePosition fallback = {});
[[nodiscard]] bool is_expression(ValueKind) noexcept;
} // namespace lvui
