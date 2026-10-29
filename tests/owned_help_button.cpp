#include <LVUI/HelpButton.h>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    auto config=lvui::help_button_configuration();lvui::Button button;
    require(lvui::Button::validate(config)&&button.set_configuration(config),"Help button preset is invalid");
    require(config.text=="?"&&config.accessibility_name=="Help"&&config.width==21&&config.height==21&&
        config.horizontal_padding==4&&config.corner_radius==100&&config.font_size==12&&config.font_weight==600,
        "Help button defaults changed");
    auto visual=button.visual({});require(visual.background==config.palette.normal&&visual.text==config.palette.text,
        "Help button did not use its own visual palette");
    auto geometry=button.geometry(21,21,7,12);require(geometry&&geometry->radius==10.5&&geometry->text.y==5,
        "Help button circular geometry or text alignment changed");
    auto invalid=config;invalid.width=0;require(!button.set_configuration(invalid)&&button.configuration()==config,
        "Invalid help button dimensions replaced the valid preset");
}
