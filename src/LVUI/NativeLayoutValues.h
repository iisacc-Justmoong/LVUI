#pragma once
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lvui::native {
enum class StackAxis { horizontal, vertical, overlay };
enum class LayoutAlignment { center, leading, trailing, stretch };
struct LayoutAlignmentAxes final {
    LayoutAlignment horizontal=LayoutAlignment::center, vertical=LayoutAlignment::center;
    friend bool operator==(const LayoutAlignmentAxes&,const LayoutAlignmentAxes&)=default;
};
struct LayoutInsets final {
    double left=0,top=0,right=0,bottom=0;
    friend bool operator==(const LayoutInsets&,const LayoutInsets&)=default;
};
inline std::string alignment_token(std::string_view text) {
    if(text.size()>32)return {};
    std::string token;token.reserve(text.size());
    for(char c:text)token+=c>='A'&&c<='Z'?char(c-'A'+'a'):c;
    return token;
}
inline std::optional<LayoutAlignmentAxes> named_stack_alignment(std::string_view text,StackAxis axis,bool strict_names=false) {
    using A=LayoutAlignment;using Axes=LayoutAlignmentAxes;
    const auto name=alignment_token(text);
    if(name=="center")return Axes{};
    if(name=="leading"&&(!strict_names||axis!=StackAxis::horizontal))
        return axis==StackAxis::horizontal?Axes{A::center,A::leading}:Axes{A::leading,A::center};
    if(name=="trailing"&&(!strict_names||axis!=StackAxis::horizontal))
        return axis==StackAxis::horizontal?Axes{A::center,A::trailing}:Axes{A::trailing,A::center};
    if(name=="top"&&(!strict_names||axis!=StackAxis::vertical))return Axes{A::center,A::leading};
    if(name=="bottom"&&(!strict_names||axis!=StackAxis::vertical))return Axes{A::center,A::trailing};
    if(!strict_names||axis==StackAxis::overlay){
        if(name=="topleading")return Axes{A::leading,A::leading};
        if(name=="toptrailing")return Axes{A::trailing,A::leading};
        if(name=="bottomleading")return Axes{A::leading,A::trailing};
        if(name=="bottomtrailing")return Axes{A::trailing,A::trailing};
    }
    if(!strict_names&&name=="stretch")return Axes{A::stretch,A::stretch};
    return {};
}
inline LayoutAlignmentAxes layout_alignment(LayoutAlignmentAxes value) {
    if(unsigned(value.horizontal)>3||unsigned(value.vertical)>3)throw std::invalid_argument("Invalid stack alignment axes");
    return value;
}
inline LayoutAlignmentAxes parse_stack_alignment(std::string_view text,StackAxis axis) {
    auto value=named_stack_alignment(text,axis);
    if(!value)throw std::invalid_argument("Invalid named stack alignment");
    return *value;
}
struct StackLayoutInput final {
    double spacing=-1,default_spacing=8,padding=0;
    std::optional<double> left_padding,top_padding,right_padding,bottom_padding;
    LayoutAlignmentAxes alignment;
    std::string_view alignment_name;
    StackAxis axis=StackAxis::vertical;
    double width=0,height=0,minimum_width=0,minimum_height=0,flex=0;
    bool intrinsic_width=false,measure_hidden_children=false;
};
struct StackLayoutValue final {
    double spacing=0,padding=0;
    std::optional<LayoutInsets> insets;
    LayoutAlignmentAxes alignment;
    double width=0,height=0,minimum_width=0,minimum_height=0,flex=0;
    bool intrinsic_width=false,measure_hidden_children=false;
    friend bool operator==(const StackLayoutValue&,const StackLayoutValue&)=default;
};
// Shared by the AOT constant folder and generated native update getters.
// Contains no widget, host, binding graph, compiler AST or GUI dependency.
inline StackLayoutValue stack_layout_value(const StackLayoutInput& input) {
    const auto range=[](double n,double low,double high){
        if(!std::isfinite(n)||n<low||n>high)throw std::invalid_argument("Invalid native stack layout input");
        return n;
    };
    if(unsigned(input.axis)>2)throw std::invalid_argument("Invalid stack axis");
    (void)range(input.spacing,-32767,32767);(void)range(input.default_spacing,0,32767);
    StackLayoutValue value{.spacing=input.spacing<0?input.default_spacing:input.spacing,
        .padding=range(input.padding,0,32767),.alignment=layout_alignment(input.alignment),
        .width=range(input.width,0,8192),.height=range(input.height,0,8192),
        .minimum_width=range(input.minimum_width,0,8192),.minimum_height=range(input.minimum_height,0,8192),
        .flex=range(input.flex,0,4096),.intrinsic_width=input.intrinsic_width,.measure_hidden_children=input.measure_hidden_children};
    if(!input.alignment_name.empty())if(auto named=named_stack_alignment(input.alignment_name,input.axis,true))value.alignment=*named;
    if(input.left_padding||input.top_padding||input.right_padding||input.bottom_padding)
        value.insets=LayoutInsets{range(input.left_padding.value_or(input.padding),0,8192),range(input.top_padding.value_or(input.padding),0,8192),
            range(input.right_padding.value_or(input.padding),0,8192),range(input.bottom_padding.value_or(input.padding),0,8192)};
    return value;
}
}
