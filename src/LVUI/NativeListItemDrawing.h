#pragma once

#include "ListItem.h"
#include "NativeVisual.h"
#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

namespace lvui::native {

struct ListItemDrawing final {
    ListItem model;
    std::shared_ptr<const ii::gui::RasterImage> icon, chevron;
};
[[nodiscard]] inline ii::gui::ActionOptions list_item_action(const ListItemDrawing& drawing) {
    const auto& c = drawing.model.configuration(); const auto size = drawing.model.intrinsic_size();
    return {.accessibility_name = c.accessibility_name.empty() ? c.label : c.accessibility_name,
            .enabled = c.enabled, .focus_ring = false, .width = size.width, .height = size.height,
            .interaction_enabled = c.interactive && c.selectable, .accessibility_description = c.description};
}
[[nodiscard]] inline ii::gui::Rect list_item_rect(ListItemRect value) noexcept {
    return {float(value.x), float(value.y), float(value.width), float(value.height)};
}
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> list_item_paint(
        const ListItemDrawing& drawing, const ii::gui::WidgetPaintState& state) {
    try {
        const auto geometry = drawing.model.geometry(state.size.width, state.size.height);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item extent");
        const auto& c = drawing.model.configuration(); auto visual = drawing.model.visual({state.hovered, state.pressed, state.focused});
        if (!state.enabled) { visual.text = c.palette.disabled_text; visual.description = c.palette.disabled_text; visual.focused = false; }
        auto result = std::make_shared<ii::gui::WidgetDrawing>();
        const auto background = native_encoded_color(visual.background);
        const auto radius = float(std::min({c.corner_radius, state.size.width / 2, state.size.height / 2}));
        if (background.alpha > 0 && state.size.width > 0 && state.size.height > 0)
            result->panels.push_back({list_item_rect(geometry->surface), background, background, radius});
        if (visual.focused) { const auto focus = native_encoded_color(visual.focus);
            result->panels.push_back({list_item_rect(geometry->surface), focus, focus, radius, 1.5f}); }
        if (drawing.icon && geometry->icon.width > 0) result->images.push_back({.bounds=list_item_rect(geometry->icon), .image=drawing.icon, .opacity=state.enabled ? 1.f : .35f});
        if (drawing.chevron && geometry->chevron.width > 0) result->images.push_back({.bounds=list_item_rect(geometry->chevron), .image=drawing.chevron, .opacity=state.enabled ? 1.f : .35f});
        const auto clip = list_item_rect(geometry->surface);
        if (!c.label.empty() && geometry->label.width > 0 && geometry->label.height > 0)
            result->texts.push_back({.bounds=list_item_rect(geometry->label), .text=c.label, .color=native_encoded_color(visual.text),
                .font_size=13, .font_weight=500, .alignment=ii::gui::TextAlignment::left, .font_family=state.font_family,
                .line_height=16, .elide_right=true, .clip=clip});
        if (geometry->description.width > 0 && geometry->description.height > 0)
            result->texts.push_back({.bounds=list_item_rect(geometry->description), .text=c.description, .color=native_encoded_color(visual.description),
                .font_size=11, .font_weight=400, .alignment=ii::gui::TextAlignment::left, .font_family=state.font_family,
                .line_height=14, .elide_right=true, .clip=clip});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
    } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item paint failed", std::current_exception()); }
}

} // namespace lvui::native
