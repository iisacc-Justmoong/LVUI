#include <LVUI/NativeVisual.h>
#include <stdexcept>
int main(){
    using lvui::native::native_display_text;
    const std::string text="A\xEA\xB0\x80" "B";
    if(native_display_text(text,1)!="A"||native_display_text(text,2)!="A"||native_display_text(text,3)!="A"||
       native_display_text(text,4)!=text.substr(0,4)||native_display_text(text,5)!=text||!native_display_text(text,0).empty())
        throw std::runtime_error("UTF-8 display limit split a scalar");
}
