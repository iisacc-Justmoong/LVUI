#pragma once

#include <LVUI/ColorChannels.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace lvui {

// Adapt one replaceable numeric channel to the generic horizontal surface
// contract. The host can reuse the same pointer, keyboard and AX input path.
class ColorChannelSurface final : public ColorSurfaceSource {
public:
    ColorChannelSurface(std::shared_ptr<ColorChannelsSource> source, ColorChannel channel)
        : source_(std::move(source)), channel_(channel) {
        if (!source_ || !ColorChannels::valid_channel(channel_)) throw std::invalid_argument("Invalid color channel source");
    }
    [[nodiscard]] ColorSourceResult color() const override { return source_->color(); }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return source_->revision(); }
    [[nodiscard]] ColorSurfaceResult<ColorSurfaceFrame> frame(
            ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const override {
        if (kind != ColorSurfaceKind::spectrum || !width || !height || width > 1024 || height > 1024) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::invalid, "Invalid color channel surface"});
        }
        const auto before = source_->revision();
        auto channel = source_->channel_frame(channel_);
        if (!channel) return std::unexpected(channel.error());
        if (!ColorChannels::valid_frame(*channel) || channel->channel != channel_) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::invalid, "Invalid color channel frame"});
        }
        if (before != channel->revision || source_->revision() != before) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::stale, "Color channel changed during rendering"});
        }
        ColorSurfaceFrame result;
        result.color = channel->color;
        result.marker = {(channel->value - channel->minimum) / (channel->maximum - channel->minimum), .5};
        result.revision = channel->revision; result.width = width; result.height = height;
        result.rgba.resize(std::size_t(width) * height * 4);
        const auto byte = [](double value) { return std::uint8_t(std::lround(std::clamp(value, 0., 1.) * 255)); };
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto position = width == 1 ? 0. : double(x) / double(width - 1) * 6;
            const auto index = std::min<std::size_t>(std::size_t(position), 5);
            const auto blend = position - double(index);
            const auto& left = channel->stops[index]; const auto& right = channel->stops[index + 1];
            const auto mix = [blend](double a, double b) { return a + (b - a) * blend; };
            const auto offset = std::size_t(x) * 4;
            result.rgba[offset] = byte(mix(left.red, right.red));
            result.rgba[offset + 1] = byte(mix(left.green, right.green));
            result.rgba[offset + 2] = byte(mix(left.blue, right.blue));
            result.rgba[offset + 3] = byte(mix(left.alpha, right.alpha));
        }
        const auto row = std::size_t(width) * 4;
        for (std::uint32_t y = 1; y < height; ++y) {
            std::copy_n(result.rgba.begin(), row, result.rgba.begin() + std::size_t(y) * row);
        }
        if (source_->revision() != before) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::stale, "Color channel changed during rendering"});
        }
        return result;
    }
    [[nodiscard]] ColorSurfaceResult<std::uint64_t> edit(
            ColorSurfaceKind kind, ColorSurfacePoint point, std::uint64_t expected_revision) override {
        if (kind != ColorSurfaceKind::spectrum || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::invalid, "Invalid color channel input"});
        }
        auto channel = source_->channel_frame(channel_);
        if (!channel) return std::unexpected(channel.error());
        if (!ColorChannels::valid_frame(*channel) || channel->channel != channel_) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::invalid, "Invalid color channel frame"});
        }
        if (channel->revision != expected_revision) {
            return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::stale, "Color channel input is stale"});
        }
        return source_->edit_channel(channel_, channel->minimum + point.x * (channel->maximum - channel->minimum), expected_revision);
    }
private:
    std::shared_ptr<ColorChannelsSource> source_;
    ColorChannel channel_;
};

} // namespace lvui
