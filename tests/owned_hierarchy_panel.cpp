#include <LVUI/HierarchyPanel.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace lvui;
    HierarchyPanelConfiguration configuration;
    auto normal = hierarchy_panel_geometry(configuration, 200, 530);
    require(normal && normal->toolbar == HierarchyPanelRect{0,0,200,26} &&
            normal->list == HierarchyPanelRect{0,26,200,504} && !normal->footer_visible,
            "Hierarchy panel lost its default size or toolbar and list arrangement");
    configuration.footer_visible = true;
    auto footer = hierarchy_panel_geometry(configuration, 300, 200);
    require(footer && footer->toolbar == HierarchyPanelRect{0,0,300,26} && footer->list == HierarchyPanelRect{0,26,300,148} &&
            footer->footer == HierarchyPanelRect{0,174,300,26} && footer->footer_visible,
            "Hierarchy panel did not place its footer at the bottom");
    auto narrow = hierarchy_panel_geometry(configuration, 10, 30);
    require(narrow && narrow->toolbar.height == 26 && narrow->list.height == 0 && narrow->footer.height == 4 &&
            narrow->footer.y == 26, "A short hierarchy panel produced negative or overlapping regions");
    configuration.toolbar_visible = false; auto no_toolbar = hierarchy_panel_geometry(configuration, 200, 100);
    require(no_toolbar && !no_toolbar->toolbar_visible && no_toolbar->list == HierarchyPanelRect{0,0,200,74} &&
            no_toolbar->footer.y == 74, "A hidden hierarchy toolbar retained layout space");
    auto empty = hierarchy_panel_geometry(configuration, 0, 0);
    require(empty && !empty->toolbar_visible && !empty->list_visible && !empty->footer_visible,
            "A zero extent hierarchy panel retained visible regions");
    auto invalid = configuration; invalid.toolbar_height = std::numeric_limits<double>::infinity();
    require(!valid_hierarchy_panel_configuration(invalid) && !hierarchy_panel_geometry(invalid, 200, 100) &&
            !hierarchy_panel_geometry(configuration, -1, 100) && !hierarchy_panel_geometry(configuration, 200, 8193),
            "Hierarchy panel accepted invalid or unbounded geometry");
}
