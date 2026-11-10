#include <LVUI/TextEditor.h>
#include <limits>
#include <stdexcept>

void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    lvui::TextEditorConfiguration value;
    check(lvui::valid_text_editor_configuration(value)&&value.field.multiline&&value.field.wrap_enabled&&
          value.field.surface==lvui::InputFieldSurface::rounded&&!value.field.maximum_length,
          "Text editor defaults are not the plain multiline contract");
    value.minimum_height=72;value.field.height=40;value.field.text="한글\n😀";
    check(value.minimum_height==72&&value.field.height==40&&value.field.text=="한글\n😀",
          "Text editor declaration changed its authored values");
    value.minimum_height=std::numeric_limits<double>::infinity();
    check(!lvui::valid_text_editor_configuration(value),"Infinite minimum height was accepted");
    value.minimum_height=72;value.field.password=true;
    check(!lvui::valid_text_editor_configuration(value),"Password mode was accepted by a plain text editor");
    value.field.password=false;value.field.multiline=false;
    check(!lvui::valid_text_editor_configuration(value),"Single-line mode was accepted by a text editor");
}
