#include <LVUI/Stepper.h>
#include <stdexcept>
int main() {
    lvui::StepperConfiguration value;
    if (value.width != 18 || value.height != 18 || value.corner_radius != 4 ||
        value.arrow != lvui::StepperArrow::up_down || value.accessibility_name != "Stepper")
        throw std::runtime_error("Stepper authored defaults changed");
    const auto copy = value; if (!(copy == value)) throw std::runtime_error("Stepper value equality changed");
}
