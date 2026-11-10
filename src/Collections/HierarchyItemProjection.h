#pragma once
#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyItem.h>
#include <optional>
#include <utility>
namespace lvui {
// The app chooses the model; the implementation owns visual projection.
template<class Model>[[nodiscard]] inline std::optional<HierarchyItemConfiguration> hierarchy_item_configuration(
        const HierarchyVisibleEntry& row,HierarchyItemConfiguration appearance={}){
 return Model::project(row,std::move(appearance));
}
} // namespace lvui
