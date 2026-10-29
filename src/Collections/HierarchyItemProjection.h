#pragma once

#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyItem.h>

#include <optional>
#include <utility>

namespace lvui {

// Presentation is derived from a view projection; neither the source nor its
// revision is mutated when a row is styled or constructed.
[[nodiscard]] inline std::optional<HierarchyItemConfiguration> hierarchy_item_configuration(
        const HierarchyVisibleEntry& row, HierarchyItemConfiguration appearance = {}) {
    appearance.key = row.entry.key; appearance.id = row.entry.id; appearance.label = row.entry.label;
    appearance.icon_name = row.entry.icon_name; appearance.count = row.entry.count; appearance.depth = row.depth;
    appearance.enabled = row.entry.enabled; appearance.activatable = row.entry.selectable;
    appearance.selected = row.selected; appearance.expanded = row.expanded; appearance.has_children = row.child_count > 0;
    if (!HierarchyItem::validate(appearance)) return std::nullopt;
    return appearance;
}

} // namespace lvui
