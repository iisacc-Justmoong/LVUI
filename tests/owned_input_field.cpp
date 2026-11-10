#include <LVUI/InputField.h>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    lvui::InputFieldConfiguration options;
    require(lvui::valid_input_field_configuration(options)&&options.width==206&&options.height==30&&
        options.horizontal_padding==8&&options.font_size==13&&options.double_click_interval==.5&&
        options.double_click_distance==4&&options.enabled&&!options.multiline&&!options.password,
        "Input field defaults changed");
    options.text="preserve";options.width=321;
    lvui::apply_input_field_size(options,lvui::InputFieldSize::compact);
    require(options.width==321&&options.text=="preserve"&&options.height==22&&options.horizontal_padding==7&&
            options.vertical_padding==4.5&&options.corner_radius==8&&options.font_size==13&&options.line_height==13,
            "Compact input did not use the shared Figma TextField dimensions");
    lvui::apply_input_field_size(options,lvui::InputFieldSize::regular);
    require(options.width==321&&options.text=="preserve"&&options.height==30&&options.horizontal_padding==8&&
            options.vertical_padding==4&&options.line_height==0,"Regular input dimensions did not recover");
    bool invalid=false;try{(void)lvui::input_field_size(2);}catch(const std::invalid_argument&){invalid=true;}
    require(invalid,"Unknown input field size was accepted");
    options.surface=lvui::input_field_surface(2);
    require(options.surface==lvui::InputFieldSurface::cylinder&&lvui::valid_input_field_configuration(options),
            "Cylinder input surface was not accepted");
    options.surface=lvui::input_field_surface(3);
    require(options.surface==lvui::InputFieldSurface::inline_&&lvui::valid_input_field_configuration(options),
            "Inline input surface was not accepted");
    options.double_click_interval=0;
    require(lvui::valid_input_field_configuration(options),"Disabled double-click selection was rejected");
    options.double_click_interval=-.1;
    require(!lvui::valid_input_field_configuration(options),"Negative double-click timing was accepted");
    options.double_click_interval=.5;options.double_click_distance=std::numeric_limits<double>::infinity();
    require(!lvui::valid_input_field_configuration(options),"Invalid double-click distance was accepted");
    options.double_click_distance=4;
    options.password=true;options.multiline=true;options.text="한😀";
    require(lvui::valid_input_field_configuration(options),"Valid international secure input was rejected");
    options.text=std::string("bad\0value",9);
    require(!lvui::valid_input_field_configuration(options),"NUL text was accepted");
    options.text="ok";options.width=std::numeric_limits<double>::infinity();
    require(!lvui::valid_input_field_configuration(options),"Invalid geometry was accepted");
    options.width=206;options.palette.text.alpha=2;
    require(!lvui::valid_input_field_configuration(options),"Invalid input field color was accepted");
}
