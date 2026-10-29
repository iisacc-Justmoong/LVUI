#pragma once
#include <LVUI/ColorSelectionSurface.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>

struct NativeOwnedColorSurfaceModel final {
    std::shared_ptr<lvui::ColorSelection> selection = std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1, 0, 0, 1});
    std::shared_ptr<lvui::ColorSelectionSurface> owner = std::make_shared<lvui::ColorSelectionSurface>(selection);
    ii::core::Observable<std::shared_ptr<lvui::ColorSurfaceSource>> source{owner};
    ii::core::Observable<std::shared_ptr<const lvui::ColorSource>> colorSource{owner};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<lvui::ColorSurfaceKind> kind{lvui::ColorSurfaceKind::saturation_brightness};
    int edits = 0;
    void edited(ii::gui::WidgetColor) { ++edits; revision.set(std::int64_t(owner->revision())); }
};
