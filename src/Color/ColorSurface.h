#pragma once

#include <LVUI/ColorSource.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

enum class ColorSurfaceKind {
    wheel, hue_saturation, saturation_brightness, spectrum, hue_rail, brightness_rail
};

struct ColorSurfacePoint final {
    double x = 0, y = 0;
    friend bool operator==(const ColorSurfacePoint&, const ColorSurfacePoint&) = default;
};

struct ColorSurfaceFrame final {
    StyleColor color;
    ColorSurfacePoint marker;
    std::uint64_t revision = 0;
    std::uint32_t width = 0, height = 0;
    // Straight RGBA8, encoded sRGB, top row first.
    std::vector<std::uint8_t> rgba;
};

enum class ColorSurfaceErrorCode { invalid, stale, unavailable };
struct ColorSurfaceError final {
    ColorSurfaceErrorCode code;
    std::string message;
};
template<class T>
using ColorSurfaceResult = std::expected<T, ColorSurfaceError>;

// Apps can replace both the color calculation and its storage. A frame is one
// consistent revision; edits must reject revisions superseded by another owner.
class ColorSurfaceSource : public ColorSource {
public:
    [[nodiscard]] virtual ColorSurfaceResult<ColorSurfaceFrame> frame(
        ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const = 0;
    [[nodiscard]] virtual ColorSurfaceResult<std::uint64_t> edit(
        ColorSurfaceKind kind, ColorSurfacePoint point, std::uint64_t revision) = 0;
};

struct ColorSurfaceConfiguration final {
    ColorSurfaceKind kind = ColorSurfaceKind::wheel;
    double width = 192, height = 192, marker_diameter = 12;
    bool enabled = true;
    std::string accessibility_name = "Choose color";
    friend bool operator==(const ColorSurfaceConfiguration&, const ColorSurfaceConfiguration&) = default;
};

// Source frames and kinds are validated independently of the graphics host.
[[nodiscard]] inline bool valid_color_surface_kind(ColorSurfaceKind kind)noexcept {
    switch(kind){case ColorSurfaceKind::wheel:case ColorSurfaceKind::hue_saturation:case ColorSurfaceKind::saturation_brightness:
    case ColorSurfaceKind::spectrum:case ColorSurfaceKind::hue_rail:case ColorSurfaceKind::brightness_rail:return true;}return false;
}
[[nodiscard]] inline bool valid_color_surface_frame(const ColorSurfaceFrame& f)noexcept {
    const auto n=[](double v){return std::isfinite(v)&&v>=0&&v<=1;};
    return f.width>0&&f.width<=1024&&f.height>0&&f.height<=1024&&f.rgba.size()==std::size_t(f.width)*f.height*4&&
        valid_color_value(f.color)&&n(f.marker.x)&&n(f.marker.y);
}
struct ColorSurfaceModelPolicy final {
    using Configuration=ColorSurfaceConfiguration;using Kind=ColorSurfaceKind;using Point=ColorSurfacePoint;using Frame=ColorSurfaceFrame;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_color(StyleColor value)noexcept{return valid_color_value(value);}
};
} // namespace lvui
