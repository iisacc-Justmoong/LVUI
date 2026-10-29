#include <LVUI/Button.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b)<1e-9; }
}
int main() {
    lvui::Button button;
    lvui::ButtonConfiguration value;
    value.text="Save";
    value.accessibility_name="저장";
    require(button.set_configuration(value),"Valid button rejected");
    const auto idle=button.visual({});
    require(idle.enabled && idle.background==value.palette.primary && idle.text==value.palette.text,
            "Primary idle style is incorrect");
    require(button.visual({.hovered=true}).background==value.palette.primary_hover,
            "Hover did not use the LVUI palette");
    require(button.visual({.hovered=true,.pressed=true}).background==value.palette.primary_pressed,
            "Pressed state did not take precedence");
    value.tone=lvui::ButtonTone::borderless;
    require(button.set_configuration(value),"Borderless button rejected");
    require(button.visual({}).background.alpha==0 && button.visual({}).text==value.palette.primary,
            "Borderless button has a surface or the wrong foreground");
    value.tone=lvui::ButtonTone::disabled;
    require(button.set_configuration(value) && !button.visual({.hovered=true,.pressed=true}).enabled,
            "Disabled tone admitted interaction");
    require(button.visual({.hovered=true,.pressed=true}).background==value.palette.disabled_background,
            "Disabled surface did not take precedence");
    value.tone=lvui::ButtonTone::normal;value.icon_mode=true;value.has_icon=true;
    require(button.set_configuration(value),"Icon button rejected");
    const auto layout=button.geometry(100,22,32,13);
    require(layout && layout->draw_icon && layout->draw_text && near(layout->icon.width,16) &&
            near(layout->text.width,32) && near(layout->text.y,4.5),"Icon/text geometry is incorrect");
    const auto long_text=button.geometry(100,22,9000,13);
    require(long_text && long_text->draw_text && near(long_text->text.width,61),
            "Long text did not fit in the available button width");
    require(!button.geometry(100,22,std::numeric_limits<double>::infinity(),13),
            "Non-finite text metrics were accepted");
    const auto before=button.configuration();
    auto invalid=value;invalid.height=0;
    require(!button.set_configuration(invalid) && button.configuration()==before,
            "Invalid update changed the last valid configuration");
    invalid=value;invalid.has_icon=false;
    require(!button.set_configuration(invalid),"Icon mode without an image was accepted");
    invalid=value;invalid.text=std::string("x\0y",3);
    require(!button.set_configuration(invalid),"Invalid text was accepted");
    invalid=value;invalid.palette.primary.red=std::numeric_limits<double>::quiet_NaN();
    require(!button.set_configuration(invalid),"Invalid palette was accepted");
    invalid=value;invalid.border_color=lvui::StyleColor{0,0,0,2};
    require(!button.set_configuration(invalid),"Invalid border color was accepted");
    invalid=value;invalid.border_width=-1;
    require(!button.set_configuration(invalid),"Invalid border width was accepted");
    invalid=value;invalid.tone=static_cast<lvui::ButtonTone>(99);
    require(!button.set_configuration(invalid),"Unknown button tone was accepted");
    require(!button.geometry(-1,22,10,13),"Invalid allocation was accepted");
}
