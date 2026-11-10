#pragma once

#include <LVUI/Text.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

struct ListItemOption final {
    std::string text;
    bool enabled=true;
    friend bool operator==(const ListItemOption&,const ListItemOption&)=default;
};

struct ListItemSelectorConfiguration final {
    std::vector<ListItemOption> options;
    int selected_index=0;
    std::string placeholder="Option";
    bool enabled=true;
    friend bool operator==(const ListItemSelectorConfiguration&,const ListItemSelectorConfiguration&)=default;
};

struct ListItemSelectorModelPolicy final {using Configuration=ListItemSelectorConfiguration;using Option=ListItemOption;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}};

} // namespace lvui
