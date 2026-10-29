#include "NativeOwnedColorSurface.generated.h"
#include "NativeOwnedColorPicker.generated.h"
#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/ColorPicker.h>
#include <LVUI/NativeColorChannel.h>
#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyViewport.h>

#include <memory>

auto* volatile color_surface_factory = &lvui_generated::NativeOwnedColorSurface::build;
auto* volatile color_picker_view_factory = &lvui_generated::NativeOwnedColorPicker::build;
std::shared_ptr<lvui::ColorSurfaceSource> make_color_surface() {
    return std::make_shared<lvui::ColorSelectionSurface>(std::make_shared<lvui::ColorSelection>());
}
auto* volatile color_surface_source_factory = &make_color_surface;
auto* volatile color_channel_factory = &lvui::native::NativeColorChannel::create;
std::unique_ptr<lvui::ColorPicker> make_color_picker() {
    return std::make_unique<lvui::ColorPicker>(std::make_shared<lvui::ColorSelection>());
}
auto* volatile color_picker_factory = &make_color_picker;
std::shared_ptr<lvui::HierarchySource> make_hierarchy_source() {
    return std::make_shared<lvui::HierarchyMemory>();
}
auto* volatile hierarchy_source_factory = &make_hierarchy_source;
auto* volatile hierarchy_viewport_factory = &lvui::HierarchyViewportSource::create;
