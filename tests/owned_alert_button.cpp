#include <LVUI/AlertButton.h>

#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    lvui::AlertButton alert;
    auto config = alert.configuration();
    auto resolved = alert.appearance();
    require(resolved.button.height == 20 && resolved.button.horizontal_padding == 8 &&
            resolved.button.corner_radius == 4 && resolved.button.tone == lvui::ButtonTone::normal && !resolved.border,
            "Standalone AlertButton defaults changed");
    require(resolved.button.palette.normal_hover.red > resolved.button.palette.normal.red &&
            resolved.button.palette.normal_pressed.red == .1555851548910141,
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
            alert.appearance().button.palette.primary == *config.primary_color,
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
