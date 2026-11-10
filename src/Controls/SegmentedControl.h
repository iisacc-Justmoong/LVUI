#pragma once
#include <LVUI/SegmentedDefaults.generated.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
namespace lvui {
enum class SegmentedShape { round_rect, cylinder };
enum class SegmentedOrientation { horizontal, vertical };
struct SegmentedControlConfiguration final {
    double horizontal_padding=segmented_defaults::label_horizontal_padding,
           vertical_padding=segmented_defaults::label_vertical_padding,
           spacing=segmented_defaults::spacing,border_width=segmented_defaults::border_width,
           corner_radius=segmented_defaults::corner_radius;
    SegmentedShape shape=SegmentedShape::round_rect;
    SegmentedOrientation orientation=SegmentedOrientation::horizontal;
    bool enabled=true,force_borderless_tone=true;
    StyleColor background{figma_defaults::panel_sub_surface};
    StyleColor border{figma_defaults::panel_surface_raised};
    friend bool operator==(const SegmentedControlConfiguration&,const SegmentedControlConfiguration&)=default;
};
[[nodiscard]] inline SegmentedControlConfiguration icon_segmented_configuration() {
    SegmentedControlConfiguration value;
    value.horizontal_padding=segmented_defaults::icon_horizontal_padding;
    value.vertical_padding=segmented_defaults::icon_vertical_padding;
    return value;
}
struct SegmentedControlGeometry final {double width=0,height=0,radius=0,border_width=0;};
} // namespace lvui
