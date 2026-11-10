#pragma once

#include <LVUI/ListItemSelector.h>
#include <LVUI/ListItemMenu.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/ListItemNavigationTokens.generated.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <any>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace lvui {

enum class ListItemVariant { mini, detail, navigation, toggle, checkable, action, action_group, stepper, select, inline_edit, detail_actions, detail_quantity, detail_settings, task, resource, media, form };
using ListItemValue = std::variant<bool, std::int64_t, double, std::string>;
struct ListItemActionValues final {
    std::string input_text1,input_text2;
    bool checked=false;
    double quantity=1;
    int selector_index=0,unit_index=0,segment_index=0;
    std::shared_ptr<const std::any> custom_payload;
    friend bool operator==(const ListItemActionValues&,const ListItemActionValues&)=default;
};
struct ListItemMenuSelection final {
    int index=0;
    ListItemOption item;
    std::vector<std::size_t> path;
    std::optional<ListItemMenuEntry> entry;
    friend bool operator==(const ListItemMenuSelection&,const ListItemMenuSelection&)=default;
};
struct ListItemInputSubmission final {
    std::string field,text;
    friend bool operator==(const ListItemInputSubmission&,const ListItemInputSubmission&)=default;
};
using ListItemActionPayload = std::variant<ListItemActionValues,ListItemMenuSelection,ListItemInputSubmission>;
struct ListItemPalette final {
    StyleColor text{1,1,1,.8}, description{1,1,1,.5}, disabled_text{1,1,1,.3};
    StyleColor idle{0,0,0,0}, hover{figma_defaults::panel_surface_alt};
    StyleColor pressed{figma_defaults::accent_blue_subtle}, selected{figma_defaults::with_alpha(figma_defaults::accent_blue_default, .30)};
    StyleColor focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ListItemPalette&, const ListItemPalette&) = default;
};
struct ListItemConfiguration final {
    ListItemVariant variant = ListItemVariant::mini;
    std::string label = "Label", description, accessibility_name, icon_name;
    std::optional<std::string> value;
    std::string detail_date,detail_folder1,detail_folder2,detail_tag1,detail_tag2;
    double width = 0, height = 0, left_padding = 4, right_padding = 4;
    double icon_size = 14, spacing = 6, corner_radius = 4, chevron_size = 14;
    double reserved_leading = 0;
    std::optional<double> reserved_leading_height;
    bool has_icon = false, enabled = true, interactive = true, selectable = true, selected = false;
    bool show_description = true, show_chevron = false, visible = true;
    bool show_value = true;
    bool show_detail_date=true,show_detail_folders=true,show_detail_tags=true;
    double value_width = list_item_navigation_tokens::value_width;
    ListItemPalette palette;
    friend bool operator==(const ListItemConfiguration&, const ListItemConfiguration&) = default;
};
struct ListItemSize final { double width = 0, height = 0; };
struct ListItemRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListItemGeometry final { ListItemRect surface, icon, label, description, chevron, value,detail_date,detail_folders,detail_tags; };
struct ListItemInteraction final { bool hovered = false, pressed = false, focused = false; };
struct ListItemVisual final { StyleColor background, text, description, focus; bool focused = false; };

} // namespace lvui
