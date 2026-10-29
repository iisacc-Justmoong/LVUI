#pragma once
#include <LVUI/Button.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

struct NativeOwnedButtonModel final {
    ii::core::Observable<std::string> text{std::string("Save")},name{std::string("Save document")};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<double> height{22},padding{8};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<ii::gui::ButtonTone> native_tone{ii::gui::ButtonTone::normal};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{};
    int saves=0,tools=0;
    void save(){++saves;}
    void tool(){++tools;}
};
