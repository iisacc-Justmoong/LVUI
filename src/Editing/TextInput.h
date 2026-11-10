#pragma once
#include <LVUI/Text.h>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {
struct TextInputRange final {
    std::size_t location = 0, length = 0;
    friend bool operator==(const TextInputRange&, const TextInputRange&) = default;
};
enum class TextInputErrorCode { invalid_text, invalid_range, invalid_boundary, unavailable, too_large, composing };
enum class TextLengthUnit { utf16_code_units, unicode_scalars };
struct TextInputError final { TextInputErrorCode code; std::string message; };
template<class T> using TextInputResult = std::expected<T,TextInputError>;

// A replaceable Unicode service. Indices and ranges use UTF-16 units, while
// byte_offset addresses UTF-8. Implementations retain their own immutable index.
class TextInputBoundary {
public:
    virtual ~TextInputBoundary() = default;
    [[nodiscard]] virtual std::size_t length() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::size_t> byte_offset(std::size_t) const = 0;
    [[nodiscard]] virtual std::size_t previous_grapheme(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t next_grapheme(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t previous_word(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t next_word(std::size_t) const noexcept = 0;
    // Optional word hit-testing; implementations without it remain usable for
    // keyboard navigation and explicitly decline pointer word selection.
    [[nodiscard]] virtual std::optional<TextInputRange> word_range(std::size_t) const noexcept { return std::nullopt; }
};
using TextInputBoundaryFactory = std::function<TextInputResult<std::shared_ptr<const TextInputBoundary>>(std::string_view)>;

struct TextInputModelPolicy final {
 using Range=TextInputRange;using ErrorCode=TextInputErrorCode;using Error=TextInputError;using LengthUnit=TextLengthUnit;
 using Boundary=TextInputBoundary;using BoundaryFactory=TextInputBoundaryFactory;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Implementation>using TextInputBinding=Implementation<TextInputModelPolicy>;

} // namespace lvui
