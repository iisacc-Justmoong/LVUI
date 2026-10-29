#pragma once
#include <LVUI/ColorPickerSession.h>
#include <LVUI/ColorSelectionSurface.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>

struct NativeOwnedColorPickerModel final {
    std::shared_ptr<lvui::ColorSelection> editor = std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1, 0, 0, .5});
    std::shared_ptr<lvui::ColorPicker> picker = std::make_shared<lvui::ColorPicker>(editor);
    std::shared_ptr<lvui::ColorSelectionSurface> source = std::make_shared<lvui::ColorSelectionSurface>(editor);
    std::shared_ptr<lvui::ColorPickerSession> owner = std::make_shared<lvui::ColorPickerSession>(picker, source, source);
    ii::core::Observable<std::shared_ptr<lvui::ColorPickerSession>> model{owner};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<lvui::ColorPickerMode> mode{lvui::ColorPickerMode::wheel};
    ii::core::Observable<double> width{320};
    int edits = 0, accepts = 0, cancels = 0, eyedroppers = 0;
    ii::gui::WidgetColor accepted_color;
    void edited(ii::gui::WidgetColor) { ++edits; revision.set(std::int64_t(editor->revision())); }
    void accepted(ii::gui::WidgetColor value) { ++accepts; accepted_color = value; revision.set(std::int64_t(editor->revision())); }
    void canceled() { ++cancels; revision.set(std::int64_t(editor->revision())); }
    void eyedropper() { ++eyedroppers; }
};
