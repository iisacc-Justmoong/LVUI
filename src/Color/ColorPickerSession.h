#pragma once

#include <LVUI/ColorChannels.h>
#include <LVUI/ColorPicker.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace lvui {

// Joins independently replaceable editing, surface and channel ports. Each
// port must describe the same selection; a gesture starts its transaction
// conditionally on the revision from which that gesture was generated.
class ColorPickerSession final : public ColorSurfaceSource, public ColorChannelsSource {
public:
    ColorPickerSession(std::shared_ptr<ColorPicker> picker,
                       std::shared_ptr<ColorSurfaceSource> surfaces,
                       std::shared_ptr<ColorChannelsSource> channels)
        : picker_(std::move(picker)), surfaces_(std::move(surfaces)), channels_(std::move(channels)) {
        if (!picker_ || !surfaces_ || !channels_) throw std::invalid_argument("Color picker requires editing, surface and channel ports");
    }

    [[nodiscard]] ColorSourceResult color() const override { return picker_->editor()->color(); }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return picker_->editor()->revision(); }

    [[nodiscard]] ColorSurfaceResult<ColorSurfaceFrame> frame(
            ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const override {
        auto current = coherent(); if (!current) return std::unexpected(current.error());
        auto result = surfaces_->frame(kind, width, height);
        if (picker_->editor()->revision() != current->revision || surfaces_->revision() != current->revision ||
            channels_->revision() != current->revision) return stale();
        if (!result) return std::unexpected(result.error());
        if (result->revision != current->revision || result->color != current->color ||
            picker_->editor()->revision() != current->revision) return stale();
        if (!ColorSurface::valid_frame(*result) || result->width != width || result->height != height)
            return invalid("Color surface returned an invalid frame");
        return result;
    }

    [[nodiscard]] ColorSurfaceResult<ColorChannelFrame> channel_frame(ColorChannel channel) const override {
        auto current = coherent(); if (!current) return std::unexpected(current.error());
        auto result = channels_->channel_frame(channel);
        if (picker_->editor()->revision() != current->revision || surfaces_->revision() != current->revision ||
            channels_->revision() != current->revision) return stale();
        if (!result) return std::unexpected(result.error());
        if (result->revision != current->revision || result->color != current->color ||
            picker_->editor()->revision() != current->revision) return stale();
        if (!ColorChannels::valid_frame(*result) || result->channel != channel)
            return invalid("Color channel returned an invalid frame");
        return result;
    }

    [[nodiscard]] ColorSurfaceResult<std::uint64_t> edit(
            ColorSurfaceKind kind, ColorSurfacePoint point, std::uint64_t expected) override {
        if (!ColorSurface::valid_kind(kind) || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1)
            return invalid("Invalid color surface input");
        auto prepared = prepare(expected); if (!prepared) return std::unexpected(prepared.error());
        return surfaces_->edit(kind, point, *prepared);
    }

    [[nodiscard]] ColorSurfaceResult<std::uint64_t> edit_channel(
            ColorChannel channel, double value, std::uint64_t expected) override {
        const auto range = ColorChannels::range(channel);
        if (!range || !std::isfinite(value) || value < range->first || value > range->second)
            return invalid("Invalid color channel input");
        auto prepared = prepare(expected); if (!prepared) return std::unexpected(prepared.error());
        return channels_->edit_channel(channel, value, *prepared);
    }

    // Called for HEX, recent colors and action buttons as well as surfaces.
    [[nodiscard]] ColorSurfaceResult<std::uint64_t> prepare(std::uint64_t expected) {
        if (owner_ != std::this_thread::get_id()) return invalid("Color picker requires its owner thread");
        if (!picker_->configuration().enabled) return invalid("Color picker is disabled");
        auto current = coherent(); if (!current) return std::unexpected(current.error());
        if (current->revision != expected) return stale();
        if (picker_->active()) return expected;
        auto started = picker_->begin_if_current(expected);
        if (!started) {
            const auto code = started.error().code == ColorSelectionErrorCode::stale
                ? ColorSurfaceErrorCode::stale : ColorSurfaceErrorCode::unavailable;
            return std::unexpected(ColorSurfaceError{code, started.error().message});
        }
        // The conditional start returns its exact revision, not a later read.
        // Sources subsequently reject an external replacement or competing edit.
        return started->revision;
    }

    [[nodiscard]] const std::shared_ptr<ColorPicker>& picker() const noexcept { return picker_; }

private:
    [[nodiscard]] static std::unexpected<ColorSurfaceError> stale() {
        return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::stale, "Color selection changed during picker input"});
    }
    [[nodiscard]] static std::unexpected<ColorSurfaceError> invalid(std::string message) {
        return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::invalid, std::move(message)});
    }
    [[nodiscard]] ColorSurfaceResult<ColorSelectionSnapshot> coherent() const {
        if (owner_ != std::this_thread::get_id()) return invalid("Color picker requires its owner thread");
        const auto current = picker_->editor()->snapshot();
        if (!valid_color_value(current.color)) return invalid("Color editor returned an invalid color");
        if (surfaces_->revision() != current.revision || channels_->revision() != current.revision) return stale();
        auto surface = surfaces_->color(), channel = channels_->color();
        if (picker_->editor()->revision() != current.revision || surfaces_->revision() != current.revision ||
            channels_->revision() != current.revision) return stale();
        if (!surface || !channel) return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::unavailable, "Color picker source is unavailable"});
        if (*surface != current.color || *channel != current.color) return invalid("Color picker ports do not describe the same color");
        return current;
    }

    std::shared_ptr<ColorPicker> picker_;
    std::shared_ptr<ColorSurfaceSource> surfaces_;
    std::shared_ptr<ColorChannelsSource> channels_;
    const std::thread::id owner_ = std::this_thread::get_id();
};

} // namespace lvui
