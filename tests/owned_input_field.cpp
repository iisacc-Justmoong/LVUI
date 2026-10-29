#include <LVUI/InputField.h>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    lvui::InputFieldConfiguration options;
    require(lvui::valid_input_field_configuration(options)&&options.width==206&&options.height==30&&
        options.horizontal_padding==8&&options.font_size==13&&options.enabled&&!options.multiline&&!options.password,
        "Input field defaults changed");
    options.password=true;options.multiline=true;options.text="한😀";
    require(lvui::valid_input_field_configuration(options),"Valid international secure input was rejected");
    options.text=std::string("bad\0value",9);
    require(!lvui::valid_input_field_configuration(options),"NUL text was accepted");
    options.text="ok";options.width=std::numeric_limits<double>::infinity();
    require(!lvui::valid_input_field_configuration(options),"Invalid geometry was accepted");
    options.width=206;options.palette.text.alpha=2;
    require(!lvui::valid_input_field_configuration(options),"Invalid input field color was accepted");
}
