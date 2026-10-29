#include <LVUI/HierarchyItem.h>

#include <limits>
#include <stdexcept>

namespace { void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); } }
int main() {
    using namespace lvui;
    HierarchyItem item; auto config = item.configuration();
    config.key = "camera"; config.label = "Camera"; config.icon_name = "home";
    config.depth = 1; config.count = 12; config.has_children = true;
    require(item.set_configuration(config), "Valid hierarchy row was rejected");
    auto geometry = item.geometry(200, 20);
    require(geometry && geometry->surface.width == 200 && geometry->icon.x == 16 && geometry->icon.width == 18 &&
            geometry->label.x == 36 && geometry->count.width == 24 && geometry->chevron.x == 176 &&
            geometry->show_count && geometry->show_icon && geometry->show_chevron,
            "Hierarchy row did not apply its inherited spacing");
    auto narrow = item.geometry(20, 20);
    require(narrow && narrow->label.width >= 0 && narrow->icon.x + narrow->icon.width <= 20 &&
            narrow->chevron.x + narrow->chevron.width <= 20, "Narrow hierarchy row produced invalid geometry");
    require(item.visual({.hovered=true}).phase == HierarchyItemPhase::hover &&
            item.visual({.hovered=true,.pressed=true}).phase == HierarchyItemPhase::pressed,
            "Hierarchy interaction precedence changed");
    config.selected = true; require(item.set_configuration(config) && item.visual({.pressed=true}).phase == HierarchyItemPhase::active,
            "Selected hierarchy row lost its active visual");
    config.enabled = false; require(item.set_configuration(config) && item.visual({.focused=true}).phase == HierarchyItemPhase::inactive &&
            !item.visual({.focused=true}).focused && !item.toggle_expansion(), "Disabled hierarchy row accepted focus or expansion");
    config.enabled = true; config.selected = false; require(item.set_configuration(config), "Unable to restore hierarchy row");
    require(item.toggle_expansion() == true && item.toggle_expansion() == false, "Chevron did not toggle expansion");
    config.has_children = false; require(item.set_configuration(config) && !item.toggle_expansion(), "A leaf expanded");
    const auto before = item.configuration(); config.width = std::numeric_limits<double>::infinity();
    require(!item.set_configuration(config) && item.configuration() == before, "Invalid row geometry changed committed options");
}
