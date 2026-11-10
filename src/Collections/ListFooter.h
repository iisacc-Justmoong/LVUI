#pragma once

#include <LVUI/ToolbarButton.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

namespace lvui {

class ListFooterCommand {
public:
    virtual ~ListFooterCommand() = default;
    virtual void invoke() const = 0;
};
struct ListFooterSlot final {
    std::string icon_name, text, accessibility_name;
    bool enabled = true, visible = true, menu_indicator = false;
    std::shared_ptr<const ListFooterCommand> command;
    friend bool operator==(const ListFooterSlot&, const ListFooterSlot&) = default;
};
struct ListFooterConfiguration final {
    std::array<ListFooterSlot, 3> slots{{
        {.icon_name="addFile", .accessibility_name="Add file"},
        {.icon_name="generaldelete", .accessibility_name="Delete"},
        {.icon_name="settings", .accessibility_name="Settings", .menu_indicator=true}}};
    double width = 0, height = 0, horizontal_padding = 2, vertical_padding = 2, spacing = 0;
    double icon_size = 18, button_height = 22, font_size = 12, corner_radius = 8, slot_width = 0;
    bool enabled = true, interactive = true;
    ToolbarButtonPalette palette;
    friend bool operator==(const ListFooterConfiguration&, const ListFooterConfiguration&) = default;
};
struct ListFooterRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListFooterGeometry final {
    double width = 0, height = 0;
    std::array<ListFooterRect, 3> slots;
};

} // namespace lvui
