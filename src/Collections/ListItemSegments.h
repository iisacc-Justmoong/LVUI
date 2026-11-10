#pragma once

#include <LVUI/ListFooter.h>
#include <LVUI/ListItem.h>
#include <LVUI/SegmentedDefaults.generated.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

struct ListItemSegment final {
    std::string text;
    bool enabled=true;
    std::shared_ptr<const ListFooterCommand> command;
    friend bool operator==(const ListItemSegment&,const ListItemSegment&)=default;
};
struct ListItemSegmentsConfiguration final {
    std::vector<ListItemSegment> options{{"Original",true,{}},{"Edited",true,{}}};
    int selected_index=0;
    bool enabled=true;
    double segment_width=56,segment_height=22,horizontal_padding=segmented_defaults::label_horizontal_padding,
           vertical_padding=segmented_defaults::label_vertical_padding,spacing=segmented_defaults::spacing;
    friend bool operator==(const ListItemSegmentsConfiguration&,const ListItemSegmentsConfiguration&)=default;
};

struct ListItemSegmentsModelPolicy final {
    using Configuration=ListItemSegmentsConfiguration;using Segment=ListItemSegment;using Size=ListItemSize;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};

} // namespace lvui
