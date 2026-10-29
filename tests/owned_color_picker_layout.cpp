#include <LVUI/ColorPickerLayout.h>

#include <array>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace lvui;
    const std::array modes{ColorPickerMode::wheel, ColorPickerMode::hue_saturation,
        ColorPickerMode::saturation_brightness, ColorPickerMode::grayscale, ColorPickerMode::rgb, ColorPickerMode::cmyk};
    const std::array heights{461., 437., 437., 361., 413., 439.};
    const std::array counts{3u, 1u, 1u, 1u, 3u, 4u};
    ColorPickerConfiguration config;
    for (std::size_t index = 0; index < modes.size(); ++index) {
        config.mode = modes[index]; auto layout = color_picker_layout(config);
        require(layout && layout->width == 320 && layout->height == heights[index] &&
                layout->channels.size() == counts[index], "Picker mode geometry changed");
        const bool plane = index == 1 || index == 2;
        require(bool(layout->rail_kind) == plane && layout->surface.width == (plane ? 260 : 296),
                "Picker plane or rail geometry is inconsistent");
        config.show_recent_colors = false; config.show_actions = false;
        const auto compact = color_picker_layout(config);
        require(compact && compact->height == heights[index] - 66 && compact->recent.empty() &&
                compact->apply.width == 0, "Hidden picker sections occupied space");
        config.show_recent_colors = config.show_actions = true;
    }
    config.mode = ColorPickerMode::hue_saturation; config.width = 480;
    auto wider = color_picker_layout(config);
    require(wider && wider->height == 597 && wider->surface.width == 420 && wider->surface.height == 420 &&
            wider->rail_kind == ColorSurfaceKind::brightness_rail && wider->channels.front().first == ColorChannel::brightness,
            "Picker plane did not grow with its width");
    config.width = 320; config.mode = ColorPickerMode::rgb;
    config.recent_colors.assign(10, {1, 0, 0, .5});
    auto recent = color_picker_layout(config);
    require(recent && recent->recent.size() == 10 && recent->recent.front().width > 0 &&
            recent->recent.back().x + recent->recent.back().width <= config.width - config.padding + 1e-8,
            "Recent color geometry did not fit its content width");
    config.padding = 64;
    require(!color_picker_layout(config), "Invalid picker padding produced geometry");
}
