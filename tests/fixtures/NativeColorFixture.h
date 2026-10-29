#pragma once
#include <iiGUI/Appearance.h>
#include <iiColorManager/ColorValue.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeColorModel final {
    static inline int live=0;
    static inline bool bad_initial=false;
    ii::core::Observable<iiColorManager::Rgba> ink{iiColorManager::Rgba{.2,.4,.6,.8}};
    ii::core::Observable<std::string> hex{"#ff000080"};
    ii::core::Observable<double> amount{.5};
    ii::core::Observable<bool> ready{true};
    ii::core::Observable<int> file_revision{0};
    NativeColorModel(){++live;if(bad_initial)ink.set({2,0,0,1});}~NativeColorModel(){--live;}
    iiColorManager::Rgba sample(double value){return {value,0,1-value,1};}
    std::string describe(ii::gui::WidgetColor value){return value.red>.5?"High":"Low";}
};
struct NativeColorPrivate final {
    static inline int live=0;
    ii::core::Observable<ii::gui::WidgetColor> ink{ii::gui::WidgetColor{0,1,0,1}};
    NativeColorPrivate(){++live;}~NativeColorPrivate(){--live;}
};
