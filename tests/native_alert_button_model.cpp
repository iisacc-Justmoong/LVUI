#include <LVUI/NativeAlertButton.h>
#include <iiGUI/AlertButtonModel.h>
namespace lvui::native {
using ButtonModel=ii::gui::BasicButton<ButtonHostModelPolicy>;
using AlertButtonModel=ii::gui::BasicAlertButton<AlertButtonModelPolicy<ButtonModel>>;
}

#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    lvui::native::AlertButtonModel alert;
    auto config = alert.configuration();
    auto resolved = alert.appearance();
    require(!config.enter_key_activation && !resolved.button.enter_key_activation,"AlertButton enabled Enter by default");
    config.enter_key_activation = true;
    require(alert.set_configuration(config) && alert.appearance().button.enter_key_activation,
            "AlertButton did not forward the Enter policy");
    require(resolved.button.height == 20 && resolved.button.horizontal_padding == 8 &&
            resolved.button.corner_radius == 4 && resolved.button.tone == lvui::ButtonTone::normal && !resolved.border,
            "Standalone AlertButton defaults changed");
    require(config.motion_enabled && resolved.button.motion_enabled,"AlertButton did not enable motion by default");
    config.motion_enabled = false;
    require(alert.set_configuration(config) && !alert.appearance().button.motion_enabled,
            "AlertButton did not forward its local motion setting");
    require(resolved.button.palette.normal_hover.red > resolved.button.palette.normal.red &&
            resolved.button.palette.normal_pressed == lvui::figma_defaults::panel_surface_input &&
            resolved.button.palette.primary_hover == lvui::figma_defaults::accent_blue_hover &&
            resolved.button.palette.primary_pressed == lvui::figma_defaults::accent_blue_pressed,
            "Standalone AlertButton state colors changed");
    config.dialog_style = true;
    require(alert.set_configuration(config), "Dialog AlertButton configuration was rejected");
    resolved = alert.appearance();
    require(resolved.button.horizontal_padding == 16 && resolved.button.corner_radius == 16 &&
            resolved.border && resolved.border_width == 1 && resolved.button.palette.normal.alpha == 0 &&
            resolved.button.palette.normal_hover.alpha == .05 && resolved.button.palette.normal_pressed.alpha == .09,
            "Dialog AlertButton did not use its outlined design");
    config.tone = lvui::ButtonTone::primary;
    config.primary_color = lvui::StyleColor{.8, .4, .2, 1};
    config.height = 56; config.horizontal_padding = 12;
    require(alert.set_configuration(config) && !alert.appearance().border &&
            alert.appearance().button.height == 56 && alert.appearance().button.horizontal_padding == 12 &&
            alert.appearance().button.palette.primary == *config.primary_color &&
            alert.appearance().button.palette.primary_hover.red == config.primary_color->red / 1.12 &&
            alert.appearance().button.palette.primary_pressed.red == config.primary_color->red / 1.2,
            "Primary AlertButton did not preserve its explicit metrics and fill");
    config.tone = lvui::ButtonTone::borderless;
    config.text_color = lvui::StyleColor{1, 0, 0, 1};
    require(alert.set_configuration(config) && alert.appearance().button.palette.primary == *config.text_color,
            "Borderless AlertButton ignored its explicit text color");
    const auto previous = alert.configuration();
    config.height = std::numeric_limits<double>::quiet_NaN();
    require(!alert.set_configuration(config) && alert.configuration() == previous,
            "Invalid AlertButton geometry changed the current configuration");
    config = previous; config.dialog_border.alpha = 2;
    require(!alert.set_configuration(config), "Invalid AlertButton border color was accepted");
}
