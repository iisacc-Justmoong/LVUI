#pragma once

#include "ColorSurface.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace lvui {

enum class ColorChannel { hue, saturation, brightness, red, green, blue, cyan, magenta, yellow, black, alpha, gray };

struct ColorChannelFrame final {
    ColorChannel channel = ColorChannel::red;
    StyleColor color;
    double value = 0, minimum = 0, maximum = 0;
    std::array<StyleColor, 7> stops;
    std::uint64_t revision = 0;
};

// A separate port keeps channel conversion replaceable. A channel frame and
// its seven ramp stops must describe the same color revision as the surface.
class ColorChannelsSource {
public:
    virtual ~ColorChannelsSource() = default;
    [[nodiscard]] virtual ColorSourceResult color() const = 0;
    [[nodiscard]] virtual std::uint64_t revision() const noexcept = 0;
    [[nodiscard]] virtual ColorSurfaceResult<ColorChannelFrame> channel_frame(ColorChannel) const = 0;
    [[nodiscard]] virtual ColorSurfaceResult<std::uint64_t> edit_channel(ColorChannel, double, std::uint64_t revision) = 0;
};

class ColorChannels final {
public:
    [[nodiscard]] static bool valid_channel(ColorChannel value) noexcept {
        switch (value) {
        case ColorChannel::hue: case ColorChannel::saturation: case ColorChannel::brightness:
        case ColorChannel::red: case ColorChannel::green: case ColorChannel::blue:
        case ColorChannel::cyan: case ColorChannel::magenta: case ColorChannel::yellow:
        case ColorChannel::black: case ColorChannel::alpha: case ColorChannel::gray: return true;
        }
        return false;
    }
    [[nodiscard]] static bool valid_frame(const ColorChannelFrame& frame) noexcept {
        const auto bounds = range(frame.channel);
        if (!bounds || !valid_color_value(frame.color) || frame.minimum != bounds->first || frame.maximum != bounds->second ||
            !std::isfinite(frame.minimum) || !std::isfinite(frame.maximum) || !std::isfinite(frame.value) ||
            frame.minimum >= frame.maximum || frame.value < frame.minimum || frame.value > frame.maximum) return false;
        return std::ranges::all_of(frame.stops, valid_color_value);
    }
    [[nodiscard]] static std::optional<std::pair<double, double>> range(ColorChannel channel) noexcept {
        if (!valid_channel(channel)) return std::nullopt;
        if (channel == ColorChannel::hue) return std::pair{0., 360.};
        if (channel == ColorChannel::red || channel == ColorChannel::green || channel == ColorChannel::blue) return std::pair{0., 255.};
        return std::pair{0., 100.};
    }
};

} // namespace lvui
