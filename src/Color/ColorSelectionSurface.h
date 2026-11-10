#pragma once

#include <LVUI/ColorSelection.h>
#include <LVUI/ColorSurface.h>
#include <LVUI/ColorChannels.h>

#include <memory>

namespace lvui {

// Application-owned composition implements this adapter. The Color contract
// does not link a color SDK. The example implementation uses iiColorManager;
// applications can instead provide another ColorSurfaceSource.
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
