#include <LVUI/Alert.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
    lvui::Alert alert;
    auto config = alert.configuration();
    require(alert.action_count() == 1 && alert.action(0).text == "Button" &&
            alert.action(0).tone == lvui::ButtonTone::primary,
            "Alert default action changed");
    config.title = "Title"; config.message = "Description";
    config.action_text = {"Apply", "Cancel", ""};
    require(alert.set_configuration(config) && alert.action_count() == 2 &&
            alert.description() == "Description", "Alert did not derive its content or action count");
    auto layout = alert.layout(800, 600, 26, 26);
    require(layout && layout->frame == lvui::AlertRect{150, 110.5, 500, 379} &&
            layout->icon == lvui::AlertRect{207, 46, 86, 86} &&
            layout->title == lvui::AlertRect{24, 160, 452, 26} &&
            layout->description == lvui::AlertRect{24, 200, 452, 26} &&
            layout->actions[1] == lvui::AlertRect{24, 290, 219, 56} &&
            layout->actions[0] == lvui::AlertRect{257, 290, 219, 56} &&
            layout->radius == 36, "Two-action alert geometry changed");
    config.action_text[2] = "Later";
    require(alert.set_configuration(config) && alert.action_count() == 3 &&
            alert.action(1).destructive && alert.action(1).tone == lvui::ButtonTone::normal,
            "Three-action alert did not select the default destructive secondary action");
    layout = alert.layout(800, 600, 26, 26);
    require(layout && layout->frame.height == 479 && layout->actions[0] == lvui::AlertRect{24, 280, 452, 56} &&
            layout->actions[1] == lvui::AlertRect{24, 348, 452, 56} &&
            layout->actions[2] == lvui::AlertRect{24, 416, 452, 44},
            "Three-action alert geometry changed");
    config.action_count = 1; config.action_text[0].clear(); config.show_icon = false;
    config.description = "Override"; config.shape = lvui::AlertShape::cylinder;
    require(alert.set_configuration(config) && alert.action_count() == 1 && alert.description() == "Override" &&
            alert.action(0).text == "Button" && !alert.action(1).visible,
            "Explicit count, empty label or description override failed");
    layout = alert.layout(300, 400, 26, 13);
    require(layout && layout->frame.width == 252 && layout->frame.x == 24 &&
            layout->icon.width == 0 && layout->radius == std::min(layout->frame.width, layout->frame.height) / 2,
            "Narrow or icon-free alert geometry failed");
    const auto old = alert.configuration();
    config.maximum_width = 0;
    require(!alert.set_configuration(config) && alert.configuration() == old,
            "Invalid width changed the current alert");
    config = old; config.palette.frame.alpha = std::numeric_limits<double>::quiet_NaN();
    require(!alert.set_configuration(config) && !alert.layout(-1, 400, 20, 20) &&
            !alert.layout(400, 400, std::numeric_limits<double>::infinity(), 20),
            "Invalid color or measured extent was accepted");
    config = old; config.action_count = 3; config.action_enabled[1] = false;
    config.secondary_destructive = false;
    require(alert.set_configuration(config) && !alert.action(1).enabled &&
            !alert.action(1).destructive && alert.action(1).tone == lvui::ButtonTone::normal,
            "Explicit secondary policy or disabled action failed");
}
