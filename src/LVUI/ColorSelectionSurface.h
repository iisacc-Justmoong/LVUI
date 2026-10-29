#pragma once

#include "ColorSelection.h"
#include "ColorSurface.h"
#include "ColorChannels.h"

#include <memory>

namespace lvui {

// LVUI owns editing and session state; this optional adapter uses iiColorManager
// for color-space calculations and raster samples. Alternative sources need
// neither this adapter nor iiColorManager's compiled library.
class ColorSelectionSurface final : public ColorSurfaceSource, public ColorChannelsSource {
public:
    explicit ColorSelectionSurface(std::shared_ptr<ColorEditing> selection);
    ~ColorSelectionSurface() override;
    ColorSelectionSurface(const ColorSelectionSurface&) = delete;
    ColorSelectionSurface& operator=(const ColorSelectionSurface&) = delete;

    [[nodiscard]] ColorSourceResult color() const override;
    [[nodiscard]] std::uint64_t revision() const noexcept override;
    [[nodiscard]] ColorSurfaceResult<ColorSurfaceFrame> frame(
        ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const override;
    [[nodiscard]] ColorSurfaceResult<std::uint64_t> edit(
        ColorSurfaceKind kind, ColorSurfacePoint point, std::uint64_t revision) override;
    [[nodiscard]] ColorSurfaceResult<ColorChannelFrame> channel_frame(ColorChannel channel) const override;
    [[nodiscard]] ColorSurfaceResult<std::uint64_t> edit_channel(ColorChannel channel, double value,
                                                                  std::uint64_t revision) override;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace lvui
