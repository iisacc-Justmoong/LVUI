#include <LVUI/NativeButton.h>
#include <iiGUI/ButtonModel.h>
namespace lvui::native {using ButtonModel=ii::gui::BasicButton<ButtonHostModelPolicy>;}
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b)<1e-9; }
}
int main() {
    lvui::native::ButtonModel button;
    auto icon=lvui::icon_button_configuration();
    icon.has_icon=true;
    require(icon.icon_mode&&near(icon.height,22)&&near(icon.icon_size,18)&&near(icon.horizontal_padding,2)&&
            near(icon.corner_radius,8)&&button.set_configuration(icon),
            "IconButton defaults do not match the LVUI Figma component");
    const auto icon_geometry=button.geometry(22,22,0,0);
    require(icon_geometry&&near(icon_geometry->icon.x,2)&&near(icon_geometry->icon.y,2)&&
            near(icon_geometry->icon.width,18)&&near(icon_geometry->icon.height,18),
            "IconButton did not place the Figma-sized icon inside its common extent");
    lvui::ButtonConfiguration value;
    value.text="Save";
    value.accessibility_name="저장";
    require(button.set_configuration(value),"Valid button rejected");
    const auto idle=button.visual({});
    require(idle.enabled && idle.background==value.palette.primary && idle.text==value.palette.text,
            "Primary idle style is incorrect");
    require(near(idle.text.red,1) && near(idle.text.green,1) && near(idle.text.blue,1) &&
            std::abs(idle.text.alpha-.9)<1e-6,
            "Button foreground does not match the Figma title text token");
    require(button.visual({.hovered=true}).background==value.palette.primary_hover,
            "Hover did not use the LVUI palette");
    require(button.visual({.hovered=true,.pressed=true}).background==value.palette.primary_pressed,
            "Pressed state did not take precedence");
    value.tone=lvui::ButtonTone::normal;
    require(button.set_configuration(value) &&
            button.visual({.pressed=true}).background==lvui::figma_defaults::accent_blue_selected,
            "Default button press does not use the selected Figma token");
    require(button.visual({}).background==lvui::figma_defaults::panel_surface_solid&&
            button.visual({.hovered=true}).background==lvui::figma_defaults::panel_surface_alt,
            "Default button did not use the Figma surface tokens");
    value.tone=lvui::ButtonTone::destructive;
    require(button.set_configuration(value)&&button.visual({}).background==lvui::figma_defaults::accent_red_default&&
            button.visual({.hovered=true}).background==lvui::figma_defaults::accent_red_hover&&
            button.visual({.pressed=true}).background==lvui::figma_defaults::accent_red_pressed,
            "Destructive button did not use the Figma red state tokens");
    auto themed=value;themed.palette.danger={.6,.3,.2,1};themed.palette.danger_reference=themed.palette.danger;
    themed.palette.danger_hover_default={.125,.25,.5,.75};themed.palette.danger_pressed_default={.75,.5,.25,.125};
    require(button.set_configuration(themed)&&button.visual({.hovered=true}).background==themed.palette.danger_hover_default&&
        button.visual({.pressed=true}).background==themed.palette.danger_pressed_default,
        "Destructive button ignored its selected common state tokens");
    themed.palette.danger={.4,.3,.2,1};
    require(button.set_configuration(themed)&&near(button.visual({.hovered=true}).background.red,.4/1.12)&&
        near(button.visual({.pressed=true}).background.red,.4/1.2),
        "Changing a themed Button's base danger color retained stale state colors");
    themed.palette.danger_hover=lvui::StyleColor{.2,.3,.4,1};
    require(button.set_configuration(themed)&&button.visual({.hovered=true}).background==*themed.palette.danger_hover,
        "An explicit danger state color lost precedence over derived and common colors");
    themed.palette.danger_reference.red=std::numeric_limits<double>::quiet_NaN();
    require(!button.set_configuration(themed),"Invalid danger reference color was accepted");
    value.palette.danger={.6,.3,.2,1};
    require(button.set_configuration(value)&&near(button.visual({.hovered=true}).background.red,.6/1.12)&&
            near(button.visual({.pressed=true}).background.red,.6/1.2),
            "Custom danger color did not retain its derived state colors");
    value.palette.danger_hover=lvui::StyleColor{.2,.3,.4,1};
    value.palette.danger_pressed=lvui::StyleColor{.1,.2,.3,1};
    require(button.set_configuration(value)&&button.visual({.hovered=true}).background==*value.palette.danger_hover&&
            button.visual({.pressed=true}).background==*value.palette.danger_pressed,
            "Explicit danger state colors did not take precedence");
    value.tone=lvui::ButtonTone::borderless;
    require(button.set_configuration(value),"Borderless button rejected");
    require(button.visual({}).background.alpha==0 && button.visual({}).text==value.palette.primary,
            "Borderless button has a surface or the wrong foreground");
    const auto selected=button.visual({.checked=true}).background;
    require(selected==lvui::figma_defaults::accent_blue_selected,
            "Default checked button does not use the selected Figma token");
    require(button.visual({.checked=true}).background==selected && button.visual({.hovered=true,.checked=true}).background==selected,
            "Checked button did not use the selected color");
    require(button.visual({.pressed=true,.checked=true}).background==value.palette.normal_pressed,
            "Press did not take precedence over selection");
    require(button.visual({.pressed=true}).background==lvui::figma_defaults::accent_blue_selected,
            "Borderless button press does not use the selected Figma token");
    require(button.configuration().tone==lvui::ButtonTone::borderless,"Selection changed the button tone");
    value.palette.primary={1,.2,.1,1};
    require(button.set_configuration(value) && button.visual({.checked=true}).background!=selected &&
            near(button.visual({.checked=true}).background.red,lvui::figma_defaults::panel_surface_interactive.red*.75+.25),
            "Selected color did not follow the changed primary color");
    lvui::native::ButtonModel injected;
    auto injected_config=lvui::ButtonConfiguration{};
    injected_config.palette.primary={.8,.2,.1,1};
    injected_config.palette.selected_reference=injected_config.palette.primary;
    injected_config.palette.selected_default={.1,.2,.3,1};
    injected_config.palette.selected_surface={.4,.5,.6,1};
    require(injected.set_configuration(injected_config)&&
            injected.visual({.checked=true}).background==injected_config.palette.selected_default,
            "Button ignored its selected color reference");
    injected_config.palette.primary={1,.2,.1,1};
    require(injected.set_configuration(injected_config)&&
            near(injected.visual({.checked=true}).background.red,.4*.75+.25),
            "Button ignored its selected surface");
    auto segment=lvui::ButtonConfiguration{};
    segment.tone=lvui::ButtonTone::borderless;
    segment.selection_style=lvui::ButtonSelectionStyle::segmented;
    require(button.set_configuration(segment),"Segment selection style was rejected");
    require(button.visual({}).background.alpha==0&&button.visual({}).text==segment.palette.primary,
            "Unselected segment did not preserve its borderless style");
    require(button.visual({.checked=true}).background==segment.palette.normal&&
            button.visual({.checked=true}).text==segment.palette.text,
            "Selected segment did not use the standard default button style");
    require(button.visual({.hovered=true,.checked=true}).background==segment.palette.normal_hover&&
            button.visual({.pressed=true,.checked=true}).background==segment.palette.normal_pressed,
            "Selected segment ignored standard hover or press states");
    segment.enabled=false;
    require(button.set_configuration(segment)&&button.visual({.checked=true}).background==segment.palette.disabled_background,
            "Segment selection overrode the disabled style");
    segment.enabled=true;segment.selection_style=static_cast<lvui::ButtonSelectionStyle>(99);
    require(!button.set_configuration(segment),"Unknown button selection style was accepted");
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
    invalid=value;invalid.palette.danger_hover=lvui::StyleColor{2,0,0,1};
    require(!button.set_configuration(invalid),"Invalid danger hover color was accepted");
    invalid=value;invalid.border_color=lvui::StyleColor{0,0,0,2};
    require(!button.set_configuration(invalid),"Invalid border color was accepted");
    invalid=value;invalid.border_width=-1;
    require(!button.set_configuration(invalid),"Invalid border width was accepted");
    invalid=value;invalid.tone=static_cast<lvui::ButtonTone>(99);
    require(!button.set_configuration(invalid),"Unknown button tone was accepted");
    require(!button.geometry(-1,22,10,13),"Invalid allocation was accepted");
}
