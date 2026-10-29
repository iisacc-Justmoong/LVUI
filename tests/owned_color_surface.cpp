#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/ColorChannelSurface.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T>
T take(lvui::ColorSurfaceResult<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
bool near(double left, double right) { return std::abs(left - right) < 1e-8; }
}

int main() {
    using namespace lvui;
    const StyleColor red{1, 0, 0, .5};
    auto selection = std::make_shared<ColorSelection>(red);
    ColorSelectionSurface source(selection);
    const std::array kinds{ColorSurfaceKind::wheel, ColorSurfaceKind::hue_saturation,
        ColorSurfaceKind::saturation_brightness, ColorSurfaceKind::spectrum,
        ColorSurfaceKind::hue_rail, ColorSurfaceKind::brightness_rail};
    for (auto kind : kinds) {
        const auto frame = take(source.frame(kind, 33, 33));
        require(ColorSurface::valid_frame(frame) && frame.color == red && frame.revision == 0,
                "A color surface returned an invalid or inconsistent frame");
    }
    const auto wheel = take(source.frame(ColorSurfaceKind::wheel, 33, 33));
    require(wheel.rgba[3] == 0 && wheel.rgba[(16 * 33 + 16) * 4 + 3] == 255,
            "Wheel raster did not preserve its transparent exterior");
    const auto plane = take(source.frame(ColorSurfaceKind::saturation_brightness, 33, 33));
    require(near(plane.marker.x, 1) && near(plane.marker.y, 0), "Red marker is misplaced");
    const auto red_channel = take(source.channel_frame(ColorChannel::red));
    require(ColorChannels::valid_frame(red_channel) && near(red_channel.value, 255) &&
            red_channel.revision == 0 && red_channel.color == red &&
            near(red_channel.stops.front().red, 0) && near(red_channel.stops.back().red, 1),
            "A color channel did not provide a consistent value and ramp");
    auto channel_source = std::make_shared<ColorSelectionSurface>(selection);
    ColorChannelSurface red_ramp(channel_source, ColorChannel::red);
    const auto ramp = take(red_ramp.frame(ColorSurfaceKind::spectrum, 13, 12));
    require(ColorSurface::valid_frame(ramp) && near(ramp.marker.x, 1) &&
            ramp.rgba[0] == 0 && ramp.rgba[(13 - 1) * 4] == 255 &&
            ramp.rgba[3] == 255, "Numeric channel did not produce a reusable gradient surface");
    auto ramp_revision = take(red_ramp.edit(ColorSurfaceKind::spectrum, {.5, .5}, selection->revision()));
    require(near(selection->snapshot().color.red, .5) &&
            !red_ramp.edit(ColorSurfaceKind::spectrum, {0, .5}, ramp_revision - 1) &&
            !red_ramp.frame(ColorSurfaceKind::wheel, 13, 12),
            "Channel surface input or stale-revision protection failed");
    require(bool(selection->set_color(red)), "Unable to reset selection after the channel ramp");

    const auto token = selection->begin();
    require(bool(token), "Shared selection could not start editing");
    auto revision = selection->revision();
    revision = take(source.edit(ColorSurfaceKind::wheel, {.5, 0}, revision));
    require(selection->snapshot().editing == *token && near(selection->snapshot().color.alpha, .5),
            "Surface edit lost the transaction or alpha");
    require(!source.edit(ColorSurfaceKind::wheel, {1, .5}, revision - 1),
            "A stale pointer event overwrote a newer color");
    require(bool(selection->cancel(*token)) && selection->snapshot().color == red,
            "Cancel did not restore the color before a shared surface edit");
    const auto channel_token = selection->begin(); require(bool(channel_token), "Channel edit session did not start");
    revision = take(source.edit_channel(ColorChannel::green, 128, selection->revision()));
    require(near(selection->snapshot().color.green, 128. / 255) && selection->snapshot().editing == *channel_token &&
            !source.edit_channel(ColorChannel::red, 0, revision - 1),
            "Channel edit lost its session or accepted a stale revision");
    require(bool(selection->cancel(*channel_token)) && selection->snapshot().color == red,
            "Cancel did not restore the color before a channel edit");

    revision = selection->revision();
    revision = take(source.edit(ColorSurfaceKind::brightness_rail, {.5, 1}, revision));
    require(selection->snapshot().color.red == 0, "Brightness rail did not reach black");
    const auto black_revision = revision;
    revision = take(source.edit(ColorSurfaceKind::wheel, {.5, 0}, revision));
    require(revision > black_revision && selection->snapshot().color.red == 0,
            "A latent hue change at black was not published");
    const auto latent_token = selection->begin();
    require(bool(latent_token), "A black color edit session could not start");
    revision = take(source.edit(ColorSurfaceKind::wheel, {0, .5}, selection->revision()));
    require(bool(selection->cancel(*latent_token)), "A latent hue edit could not be canceled");
    revision = selection->revision();
    revision = take(source.edit(ColorSurfaceKind::brightness_rail, {.5, 0}, revision));
    const auto restored = selection->snapshot().color;
    require(near(restored.red, .5) && near(restored.green, 1) && near(restored.blue, 0),
            "Hue was lost when brightness returned from black");
    const auto accepted_token = selection->begin();
    require(bool(accepted_token), "A second surface session could not start");
    revision = take(source.edit(ColorSurfaceKind::brightness_rail, {.5, 1}, selection->revision()));
    revision = take(source.edit(ColorSurfaceKind::wheel, {0, .5}, revision));
    require(bool(selection->accept(*accepted_token)), "The latent hue edit could not be accepted");
    revision = take(source.edit(ColorSurfaceKind::brightness_rail, {.5, 0}, selection->revision()));
    const auto accepted = selection->snapshot().color;
    require(near(accepted.red, 0) && near(accepted.green, 1) && near(accepted.blue, 1),
            "Accept discarded the latent hue");
    (void)revision;

    require(!source.frame(ColorSurfaceKind::wheel, 0, 4) &&
            !source.frame(ColorSurfaceKind::wheel, 1025, 4) &&
            !source.frame(static_cast<ColorSurfaceKind>(99), 4, 4), "Invalid raster requests were accepted");
    require(!source.edit(ColorSurfaceKind::wheel, {std::numeric_limits<double>::quiet_NaN(), 0}, selection->revision()),
            "Nonfinite pointer input was accepted");
    require(!source.channel_frame(static_cast<ColorChannel>(99)) &&
            !source.edit_channel(ColorChannel::red, std::numeric_limits<double>::quiet_NaN(), selection->revision()) &&
            !source.edit_channel(ColorChannel::red, 256, selection->revision()),
            "Invalid channel requests were accepted");
    for (auto value : {ColorChannel::hue, ColorChannel::saturation, ColorChannel::brightness,
                       ColorChannel::red, ColorChannel::green, ColorChannel::blue, ColorChannel::cyan,
                       ColorChannel::magenta, ColorChannel::yellow, ColorChannel::black,
                       ColorChannel::alpha, ColorChannel::gray}) {
        require(ColorChannels::valid_frame(take(source.channel_frame(value))), "A channel frame was invalid");
    }
    require(bool(selection->set_color(red)), "Unable to reset channel selection");
    revision = take(source.edit_channel(ColorChannel::brightness, 0, selection->revision()));
    const auto black_session = selection->begin(); require(bool(black_session), "Black channel edit did not start");
    const auto before_hue = selection->revision();
    revision = take(source.edit_channel(ColorChannel::hue, 240, before_hue));
    require(revision > before_hue && selection->snapshot().color.red == 0,
            "A latent hue channel edit at black was not published");
    require(bool(selection->cancel(*black_session)), "Channel edit cancellation failed");
    (void)take(source.edit_channel(ColorChannel::brightness, 100, selection->revision()));
    require(near(selection->snapshot().color.red, 1) && near(selection->snapshot().color.green, 0),
            "Channel cancellation did not restore the hidden hue");
    ColorSurface model;
    require(model.position(-10, 50, 100, 100) == ColorSurfacePoint{0, .5},
            "Pointer position was not clamped to the surface");
    auto config = model.configuration(); config.enabled = false;
    require(model.set_configuration(config) && !model.position(1, 1, 100, 100),
            "Disabled surface accepted pointer input");
    config.width = 0;
    require(!model.set_configuration(config) && model.configuration().width == 192,
            "Invalid surface configuration changed the current state");
    auto malformed = wheel; malformed.rgba.pop_back();
    require(!ColorSurface::valid_frame(malformed), "A malformed image frame was accepted");
}
