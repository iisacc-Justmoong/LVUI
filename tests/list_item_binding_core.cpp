#include <LVUI/NativeListItemDrawing.h>
#include <concepts>
#include <stdexcept>
struct Model{};struct Encoded{double red,green,blue,alpha;};
struct Converter{static Encoded convert(lvui::StyleColor value){return {value.red,value.green,value.blue,value.alpha};}};
int main(){
    using Policy=lvui::native::ListItemDrawingPolicy<Model,Converter>;
    static_assert(std::same_as<Policy::Model,Model>);
    auto color=Policy::encoded_color({.1,.2,.3,.4});
    if(color.red!=.1||color.alpha!=.4||Policy::value_font_size({})!=lvui::list_item_navigation_tokens::value_font_size||
       Policy::value_line_height({})!=lvui::list_item_navigation_tokens::value_line_height)
        throw std::runtime_error("Independent list drawing policy changed design values");
}
