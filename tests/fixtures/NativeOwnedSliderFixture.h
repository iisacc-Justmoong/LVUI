#pragma once
#include <LVUI/Slider.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedSliderModel final {
    ii::core::Observable<double> value{5},maximum{10},width{100};
    ii::core::Observable<bool> enabled{true},active{true},showTicks{false},showSymbol{false};
    ii::core::Observable<bool> showMinMax{false},showLabels{true},showEndpointIcons{false},rightToLeft{false};
    ii::core::Observable<double> tickCount{11},segmentCount{5};
    ii::core::Observable<lvui::StyleColor> labelColor{lvui::figma_defaults::text_body_color};
    ii::core::Observable<lvui::StyleColor> inactiveFillColor{lvui::figma_defaults::text_disabled_color};
    ii::core::Observable<std::string> symbolName{"sliderSymbolSun"};
    ii::core::Observable<std::string> minimumLabel{"0%"},maximumLabel{"100%"},
        minimumIconName{"sliderSymbolSun"},maximumIconName{"sliderSymbolSun"};
    ii::core::Observable<lvui::SliderSize> size{lvui::SliderSize::mini};
    ii::core::Observable<lvui::SliderType> type{lvui::SliderType::normal};
    std::vector<double> values;
    void moved(double next){values.push_back(next);value.set(next);}
};
