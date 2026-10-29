#include <LVUI/ColorSelectionSurface.h>

#include <iiColorManager/ColorManager.h>

#include <cmath>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace lvui {
namespace {

[[nodiscard]] std::optional<iiColorManager::Surface> surface(ColorSurfaceKind kind) noexcept {
    using Surface = iiColorManager::Surface;
    switch (kind) {
    case ColorSurfaceKind::wheel: return Surface::ColorWheel;
    case ColorSurfaceKind::hue_saturation: return Surface::BrightnessCube;
    case ColorSurfaceKind::saturation_brightness: return Surface::HueCube;
    case ColorSurfaceKind::spectrum: return Surface::Spectrum;
    case ColorSurfaceKind::hue_rail: return Surface::HueRail;
    case ColorSurfaceKind::brightness_rail: return Surface::BrightnessRail;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<iiColorManager::Channel> channel(ColorChannel value) noexcept {
    using Channel = iiColorManager::Channel;
    switch (value) {
    case ColorChannel::hue: return Channel::Hue;
    case ColorChannel::saturation: return Channel::Saturation;
    case ColorChannel::brightness: return Channel::Brightness;
    case ColorChannel::red: return Channel::Red;
    case ColorChannel::green: return Channel::Green;
    case ColorChannel::blue: return Channel::Blue;
    case ColorChannel::cyan: return Channel::Cyan;
    case ColorChannel::magenta: return Channel::Magenta;
    case ColorChannel::yellow: return Channel::Yellow;
    case ColorChannel::black: return Channel::Black;
    case ColorChannel::alpha: return Channel::Alpha;
    case ColorChannel::gray: return Channel::Gray;
    }
    return std::nullopt;
}

[[nodiscard]] iiColorManager::Rgba to_color(StyleColor value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}

[[nodiscard]] StyleColor from_color(iiColorManager::Rgba value) noexcept {
    return {value.r, value.g, value.b, value.a};
}

[[nodiscard]] ColorSurfaceError failure(ColorSurfaceErrorCode code, const char* message) {
    return {code, message};
}

} // namespace

struct ColorSelectionSurface::State final {
    explicit State(std::shared_ptr<ColorEditing> value) : selection(std::move(value)) {}

    void synchronize(const ColorSelectionSnapshot& snapshot) {
        if (session && session != snapshot.editing) {
            if (snapshot.last_completion && snapshot.last_completion->token == *session &&
                snapshot.last_completion->termination == ColorEditTermination::canceled && session_baseline) {
                picker = *session_baseline;
                last_color = from_color(picker.color());
            } else if (!snapshot.last_completion || snapshot.last_completion->token != *session) {
                // More than one unseen session ended. Do not retain a latent
                // hue whose owner can no longer be identified.
                picker = iiColorManager::Picker{};
                last_color.reset();
            }
            session.reset();
            session_baseline.reset();
        }
        if (snapshot.editing && !session) {
            if (!last_color || *last_color != snapshot.baseline) {
                (void)picker.editColor(to_color(snapshot.baseline));
                last_color = snapshot.baseline;
            }
            session = snapshot.editing;
            session_baseline = picker;
        }
        if (!last_color || *last_color != snapshot.color) {
            (void)picker.editColor(to_color(snapshot.color));
            last_color = snapshot.color;
        }
    }

    std::shared_ptr<ColorEditing> selection;
    mutable std::mutex mutex;
    iiColorManager::Picker picker;
    std::optional<StyleColor> last_color;
    std::optional<ColorEditToken> session;
    std::optional<iiColorManager::Picker> session_baseline;
};

ColorSelectionSurface::ColorSelectionSurface(std::shared_ptr<ColorEditing> selection) {
    if (!selection) throw std::invalid_argument("Color selection is required");
    state_ = std::make_unique<State>(std::move(selection));
}

ColorSelectionSurface::~ColorSelectionSurface() = default;

ColorSourceResult ColorSelectionSurface::color() const {
    return state_->selection->color();
}

std::uint64_t ColorSelectionSurface::revision() const noexcept {
    return state_->selection->revision();
}

ColorSurfaceResult<ColorSurfaceFrame> ColorSelectionSurface::frame(
        ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const {
    const auto mapped = surface(kind);
    if (!mapped || width == 0 || height == 0 || width > 1024 || height > 1024) {
        return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Invalid color surface request"));
    }
    std::scoped_lock lock(state_->mutex);
    const auto snapshot = state_->selection->snapshot();
    state_->synchronize(snapshot);
    auto image = iiColorManager::renderSurface(state_->picker, *mapped, width, height);
    if (!image) {
        return std::unexpected(failure(ColorSurfaceErrorCode::unavailable, "Color surface could not be rendered"));
    }
    const auto point = state_->picker.position(*mapped);
    if (state_->selection->revision() != snapshot.revision) {
        return std::unexpected(failure(ColorSurfaceErrorCode::stale, "Color selection changed during rendering"));
    }
    ColorSurfaceFrame result{snapshot.color, {point.x, point.y}, snapshot.revision,
                             image->width, image->height, std::move(image->rgba)};
    if (!ColorSurface::valid_frame(result)) {
        return std::unexpected(failure(ColorSurfaceErrorCode::unavailable, "Color calculation produced an invalid frame"));
    }
    return result;
}

ColorSurfaceResult<std::uint64_t> ColorSelectionSurface::edit(
        ColorSurfaceKind kind, ColorSurfacePoint point, std::uint64_t revision) {
    const auto mapped = surface(kind);
    const auto normalized = [](double value) {
        return std::isfinite(value) && value >= 0 && value <= 1;
    };
    if (!mapped || !normalized(point.x) || !normalized(point.y)) {
        return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Invalid color surface edit"));
    }
    std::scoped_lock lock(state_->mutex);
    const auto snapshot = state_->selection->snapshot();
    if (snapshot.revision != revision) {
        return std::unexpected(failure(ColorSurfaceErrorCode::stale, "Color selection changed before the edit"));
    }
    state_->synchronize(snapshot);
    auto candidate = state_->picker;
    if (!candidate.editSurface(*mapped, point.x, point.y)) {
        return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Color calculation rejected the edit"));
    }
    const auto color = from_color(candidate.color());
    const auto before = state_->picker.hsb(), after = candidate.hsb();
    const bool representation_changed = before.h != after.h || before.s != after.s || before.b != after.b;
    auto result = state_->selection->edit_if_current(revision, color, representation_changed);
    if (!result) {
        const auto code = result.error().code == ColorSelectionErrorCode::stale
            ? ColorSurfaceErrorCode::stale : ColorSurfaceErrorCode::unavailable;
        return std::unexpected(ColorSurfaceError{code, result.error().message});
    }
    state_->picker = std::move(candidate);
    state_->last_color = color;
    return *result;
}

ColorSurfaceResult<ColorChannelFrame> ColorSelectionSurface::channel_frame(ColorChannel value) const {
    const auto mapped = channel(value);
    const auto* info = mapped ? iiColorManager::channelInfo(*mapped) : nullptr;
    if (!info) return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Invalid color channel"));
    std::scoped_lock lock(state_->mutex);
    const auto snapshot = state_->selection->snapshot();
    state_->synchronize(snapshot);
    ColorChannelFrame result{value, snapshot.color, state_->picker.channelValue(*mapped),
                             info->minimum, info->maximum, {}, snapshot.revision};
    for (std::size_t index = 0; index < result.stops.size(); ++index) {
        result.stops[index] = from_color(state_->picker.surfaceColor(iiColorManager::Surface::ChannelRamp,
            double(index) / double(result.stops.size() - 1), .5, *mapped));
    }
    if (state_->selection->revision() != snapshot.revision) {
        return std::unexpected(failure(ColorSurfaceErrorCode::stale, "Color selection changed during channel measurement"));
    }
    if (!ColorChannels::valid_frame(result)) {
        return std::unexpected(failure(ColorSurfaceErrorCode::unavailable, "Color calculation produced an invalid channel"));
    }
    return result;
}

ColorSurfaceResult<std::uint64_t> ColorSelectionSurface::edit_channel(
        ColorChannel value, double next_value, std::uint64_t revision) {
    const auto mapped = channel(value);
    const auto* info = mapped ? iiColorManager::channelInfo(*mapped) : nullptr;
    if (!info || !std::isfinite(next_value) || next_value < info->minimum || next_value > info->maximum) {
        return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Invalid color channel edit"));
    }
    std::scoped_lock lock(state_->mutex);
    const auto snapshot = state_->selection->snapshot();
    if (snapshot.revision != revision) {
        return std::unexpected(failure(ColorSurfaceErrorCode::stale, "Color selection changed before the channel edit"));
    }
    state_->synchronize(snapshot);
    auto candidate = state_->picker;
    if (!candidate.edit(*mapped, next_value)) {
        return std::unexpected(failure(ColorSurfaceErrorCode::invalid, "Color calculation rejected the channel edit"));
    }
    const auto color = from_color(candidate.color());
    const auto before = state_->picker.hsb(), after = candidate.hsb();
    const bool representation_changed = before.h != after.h || before.s != after.s || before.b != after.b;
    auto result = state_->selection->edit_if_current(revision, color, representation_changed);
    if (!result) {
        const auto code = result.error().code == ColorSelectionErrorCode::stale
            ? ColorSurfaceErrorCode::stale : ColorSurfaceErrorCode::unavailable;
        return std::unexpected(ColorSurfaceError{code, result.error().message});
    }
    state_->picker = std::move(candidate); state_->last_color = color;
    return *result;
}

} // namespace lvui
