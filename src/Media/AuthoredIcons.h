#pragma once

#include <LVUI/IconCatalog.h>
#include <LVUI/DesignIconDefaults.generated.h>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace lvui {
// Intrinsic extent of the current LVUI design icon instances, in logical pixels.
inline constexpr double design_icon_default_extent = design_icon_defaults::extent;
// Immutable, application-independent snapshot of the authored design assets.
// The source lazily shares each resolved SVG and accepts the original aliases.
[[nodiscard]] std::shared_ptr<const IconSource> authored_icon_catalog();
[[nodiscard]] std::span<const std::string_view> authored_icon_names() noexcept;
// Exact canonical lookup without alias normalization; the view is process-lived.
[[nodiscard]] IconResult<std::string_view> authored_icon_svg(std::string_view canonical_name);
// Current LVUI design assets take priority; remaining names use the immutable
// authored catalog. Both sources share the same name and alias normalization.
[[nodiscard]] std::shared_ptr<const IconSource> design_icon_catalog();
[[nodiscard]] std::span<const std::string_view> design_icon_names() noexcept;
[[nodiscard]] IconResult<std::string_view> design_icon_svg(std::string_view canonical_name);
namespace detail {
// Only assets owned by the immutable authored catalog can use its fallback.
[[nodiscard]] std::optional<std::string_view> authored_icon_render_fallback(
    const std::shared_ptr<const IconAsset>& asset);
}
} // namespace lvui
