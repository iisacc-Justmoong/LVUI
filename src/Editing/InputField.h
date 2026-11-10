#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/SearchFieldTokens.generated.h>
#include <LVUI/TextInput.h>
#include <cmath>
#include <concepts>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace lvui {
enum class InputFieldSize { regular, compact };
enum class InputFieldSurface { flat, rounded, cylinder, inline_ };
inline InputFieldSurface input_field_surface(InputFieldSurface surface) {
    switch(surface){case InputFieldSurface::flat:case InputFieldSurface::rounded:case InputFieldSurface::cylinder:case InputFieldSurface::inline_:return surface;}
    throw std::invalid_argument("Invalid InputField surface");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
InputFieldSurface input_field_surface(T surface) {
    if(surface==0)return InputFieldSurface::flat;
    if(surface==1)return InputFieldSurface::rounded;
    if(surface==2)return InputFieldSurface::cylinder;
    if(surface==3)return InputFieldSurface::inline_;
    throw std::invalid_argument("Invalid InputField surface");
}
inline InputFieldSize input_field_size(InputFieldSize size) {
    switch(size){case InputFieldSize::regular:case InputFieldSize::compact:return size;}
    throw std::invalid_argument("Invalid InputField size");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
InputFieldSize input_field_size(T size) {
    if(size==0)return InputFieldSize::regular;
    if(size==1)return InputFieldSize::compact;
    throw std::invalid_argument("Invalid InputField size");
}
struct InputFieldPalette final {
    StyleColor background{figma_defaults::panel_surface_input};
    StyleColor hover{figma_defaults::panel_surface_input_hover};
    StyleColor text{1,1,1,.9};
    StyleColor placeholder{1,1,1,.5};
    StyleColor selection{figma_defaults::with_alpha(figma_defaults::accent_blue_default, .38)};
    StyleColor caret{1,1,1,1};
    StyleColor focus{figma_defaults::accent_blue_default};
    friend bool operator==(const InputFieldPalette&,const InputFieldPalette&)=default;
};
struct InputFieldConfiguration final {
    std::string text,placeholder,accessibility_name="Text field",font_family,clear_accessibility_name="Clear input";
    double width=206,height=30,horizontal_padding=8,vertical_padding=4,corner_radius=8,font_size=13,line_height=0;
    bool enabled=true,read_only=false,multiline=false,password=false,wrap_enabled=true,clear_button_visible=false;
    InputFieldSurface surface=InputFieldSurface::flat;
    std::optional<std::size_t> maximum_length;
    TextLengthUnit length_unit=TextLengthUnit::utf16_code_units;
    InputFieldPalette palette;
    // Used when a platform does not report click counts, or when the native
    // count is explicitly disabled. Interval zero disables double-click.
    double double_click_interval=.5,double_click_distance=4;
    bool use_native_click_count=true;
    bool reveal_caret_in_ancestors=false;
    friend bool operator==(const InputFieldConfiguration&,const InputFieldConfiguration&)=default;
};
inline void apply_input_field_size(InputFieldConfiguration& value,InputFieldSize size) {
    switch(input_field_size(size)) {
    case InputFieldSize::regular: {
        const InputFieldConfiguration defaults;
        value.height=defaults.height;value.horizontal_padding=defaults.horizontal_padding;
        value.vertical_padding=defaults.vertical_padding;value.corner_radius=defaults.corner_radius;
        value.font_size=defaults.font_size;value.line_height=defaults.line_height;
        break;
    }
    case InputFieldSize::compact:
        // The text and search variants share these Figma TextField metrics.
        value.height=search_field_defaults::height;
        value.horizontal_padding=search_field_defaults::horizontal_padding;
        value.vertical_padding=search_field_defaults::vertical_padding;
        value.corner_radius=search_field_defaults::radius;
        value.font_size=search_field_defaults::font_size;
        value.line_height=search_field_defaults::line_height;
        break;
    }
}
[[nodiscard]] inline bool valid_input_field_configuration(const InputFieldConfiguration& value) noexcept {
    const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
    const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
    return valid_accessibility_text(value.text)&&valid_accessibility_text(value.placeholder)&&
        valid_accessibility_text(value.accessibility_name)&&valid_accessibility_text(value.clear_accessibility_name)&&
        valid_accessibility_text(value.font_family)&&value.font_family.size()<=256&&
        range(value.width,1,8192)&&range(value.height,1,8192)&&range(value.horizontal_padding,0,8192)&&
        range(value.vertical_padding,0,8192)&&range(value.corner_radius,0,8192)&&range(value.font_size,1,256)&&
        range(value.line_height,0,1024)&&range(value.double_click_interval,0,2)&&range(value.double_click_distance,0,128)&&
        (value.length_unit==TextLengthUnit::utf16_code_units||value.length_unit==TextLengthUnit::unicode_scalars)&&
        (value.surface==InputFieldSurface::flat||value.surface==InputFieldSurface::rounded||
         value.surface==InputFieldSurface::cylinder||value.surface==InputFieldSurface::inline_)&&
        color(value.palette.background)&&color(value.palette.hover)&&color(value.palette.text)&&
        color(value.palette.placeholder)&&color(value.palette.selection)&&color(value.palette.caret)&&color(value.palette.focus);
}
} // namespace lvui
