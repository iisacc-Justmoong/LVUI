#pragma once
#include <LVUI/NativeLayoutValues.h>
#include <LVUI/NativeValues.h>
#include <iiGUI/WidgetLayout.h>

namespace lvui::native {
inline LayoutAlignmentAxes stack_alignment(LayoutAlignmentAxes value,StackAxis) {
    return layout_alignment(value);
}
inline LayoutAlignmentAxes stack_alignment(ii::gui::StackAlignmentAxes value,StackAxis) {
    return layout_alignment({static_cast<LayoutAlignment>(value.horizontal),static_cast<LayoutAlignment>(value.vertical)});
}
inline LayoutAlignmentAxes stack_alignment(ii::gui::StackAlignment value,StackAxis axis) {
    return stack_alignment(ii::gui::StackAlignmentAxes{value,value},axis);
}
template<String T>LayoutAlignmentAxes stack_alignment(const T& value,StackAxis axis) {
    return parse_stack_alignment(lvui::native::string_view(value),axis);
}
inline ii::gui::StackLayoutOptions stack_layout(const StackLayoutInput& input) {
    const auto value=stack_layout_value(input);
    const auto horizontal=static_cast<ii::gui::StackAlignment>(value.alignment.horizontal);
    const auto vertical=static_cast<ii::gui::StackAlignment>(value.alignment.vertical);
    ii::gui::StackLayoutOptions result{
        .stack={.spacing=value.spacing,.padding=value.padding,
                .alignment=horizontal==vertical?horizontal:ii::gui::StackAlignment::center,
                .minimum_width=value.minimum_width,.minimum_height=value.minimum_height,
                .intrinsic_width=value.intrinsic_width,.measure_hidden_children=value.measure_hidden_children},
        .extent={value.width,value.height},.flex=value.flex};
    if(horizontal!=vertical){result.stack.horizontal_alignment=horizontal;result.stack.vertical_alignment=vertical;}
    if(value.insets)result.stack.insets=ii::gui::Insets{value.insets->left,value.insets->top,value.insets->right,value.insets->bottom};
    return result;
}
}
