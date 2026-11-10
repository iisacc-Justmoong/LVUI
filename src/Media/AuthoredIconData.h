#pragma once

#include <span>
#include <string_view>
#include <utility>

namespace lvui::detail {
struct AuthoredIconRecord final { std::string_view name, svg; };
struct AuthoredIconFallback final { std::string_view name, svg; };
using AuthoredIconAlias = std::pair<std::string_view, std::string_view>;
[[nodiscard]] std::span<const AuthoredIconRecord> authored_icon_records() noexcept;
[[nodiscard]] std::span<const AuthoredIconRecord> design_icon_records() noexcept;
[[nodiscard]] std::span<const std::string_view> design_icon_record_names() noexcept;
[[nodiscard]] std::span<const AuthoredIconFallback> authored_icon_fallbacks() noexcept;
[[nodiscard]] std::span<const std::string_view> authored_icon_record_names() noexcept;
[[nodiscard]] std::span<const AuthoredIconAlias> authored_resource_aliases() noexcept;
[[nodiscard]] std::span<const AuthoredIconAlias> authored_name_aliases() noexcept;
} // namespace lvui::detail
