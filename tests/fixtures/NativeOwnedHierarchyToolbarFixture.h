#pragma once
#include <LVUI/HierarchyToolbar.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct NativeOwnedHierarchyToolbarModel final {
    ii::core::Observable<std::vector<lvui::HierarchyToolbarItem>> items{std::vector<lvui::HierarchyToolbarItem>{
        {.id=lvui::ToolbarButtonId{std::string("home")}, .icon_name="home", .accessibility_name="Home tool"},
        {.id=lvui::ToolbarButtonId{std::string("add")}, .icon_name="generaladd", .accessibility_name="Add tool"}}};
    ii::core::Observable<std::optional<lvui::ToolbarButtonId>> active{std::nullopt};
    ii::core::Observable<double> width{200};
    int changes = 0, actions = 0; std::optional<lvui::HierarchyToolbarSelection> last;
    void changed(const lvui::HierarchyToolbarSelection& value) { ++changes; last = value; active.set(value.id); }
    void triggered(const lvui::HierarchyToolbarSelection& value) { ++actions; last = value; }
};
