#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <string>
struct NativeOwnedPopoverModel final {
    ii::core::Observable<double> width{180},padding{8},opacity{.25};
    ii::core::Observable<bool> modal{false};
    ii::core::Observable<ii::gui::WidgetColor> color{{.2,.2,.2,1}};
    ii::core::Observable<std::string> text{"Details"};
    int opens=0,closes=0,cancels=0;
    std::function<void()> onClose;
    void opened(){++opens;}
    void closed(){++closes;auto callback=onClose;if(callback)callback();}
    void canceled(){++cancels;}
};
