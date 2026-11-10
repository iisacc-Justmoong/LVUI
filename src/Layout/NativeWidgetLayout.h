#pragma once
#include <LVUI/NativeLayoutValues.h>
#include <LVUI/NativeValues.h>
#include <type_traits>

namespace lvui::native {
inline LayoutAlignmentAxes stack_alignment(LayoutAlignmentAxes value, StackAxis) {
    return layout_alignment(value);
}
template<class Alignment>
concept HostStackAlignment = std::is_enum_v<Alignment> && requires {
    Alignment::center; Alignment::leading; Alignment::trailing; Alignment::stretch;
};
template<HostStackAlignment Alignment>
[[nodiscard]] inline LayoutAlignment from_host_alignment(Alignment value) {
    if (value == Alignment::center) return LayoutAlignment::center;
    if (value == Alignment::leading) return LayoutAlignment::leading;
    if (value == Alignment::trailing) return LayoutAlignment::trailing;
    if (value == Alignment::stretch) return LayoutAlignment::stretch;
    throw std::invalid_argument("Invalid host stack alignment");
}
template<HostStackAlignment Alignment>
[[nodiscard]] inline Alignment to_host_alignment(LayoutAlignment value) {
    switch (value) {
    case LayoutAlignment::center: return Alignment::center;
    case LayoutAlignment::leading: return Alignment::leading;
    case LayoutAlignment::trailing: return Alignment::trailing;
    case LayoutAlignment::stretch: return Alignment::stretch;
    }
    throw std::invalid_argument("Invalid stack alignment");
}
template<HostStackAlignment Alignment>
[[nodiscard]] inline LayoutAlignmentAxes stack_alignment(Alignment value, StackAxis) {
    const auto converted = from_host_alignment(value);
    return {converted, converted};
}
template<class Axes>
    requires requires(const Axes& value) { from_host_alignment(value.horizontal); from_host_alignment(value.vertical); }
[[nodiscard]] inline LayoutAlignmentAxes stack_alignment(const Axes& value, StackAxis) {
    return {from_host_alignment(value.horizontal), from_host_alignment(value.vertical)};
}
template<String T> LayoutAlignmentAxes stack_alignment(const T& value, StackAxis axis) {
    return parse_stack_alignment(lvui::native::string_view(value), axis);
}
template<class HostLayout>
[[nodiscard]] inline HostLayout stack_layout(const StackLayoutInput& input) {
    const auto value = stack_layout_value(input);
    HostLayout result{};
    using Alignment = std::remove_cvref_t<decltype(result.stack.alignment)>;
    using Insets = typename std::remove_cvref_t<decltype(result.stack.insets)>::value_type;
    const auto horizontal = to_host_alignment<Alignment>(value.alignment.horizontal);
    const auto vertical = to_host_alignment<Alignment>(value.alignment.vertical);
    result.stack.spacing = value.spacing; result.stack.padding = value.padding;
    result.stack.alignment = horizontal == vertical ? horizontal : Alignment::center;
    result.stack.minimum_width = value.minimum_width; result.stack.minimum_height = value.minimum_height;
    result.stack.intrinsic_width = value.intrinsic_width;
    result.stack.measure_hidden_children = value.measure_hidden_children;
    result.extent = {value.width, value.height}; result.flex = value.flex;
    if (horizontal != vertical) {
        result.stack.horizontal_alignment = horizontal; result.stack.vertical_alignment = vertical;
    }
    if (value.insets) result.stack.insets = Insets{value.insets->left, value.insets->top, value.insets->right, value.insets->bottom};
    return result;
}
} // namespace lvui::native
