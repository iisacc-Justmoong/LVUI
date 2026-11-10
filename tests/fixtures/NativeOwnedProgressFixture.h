#pragma once
#include <iiGUI/ProgressIndicator.h>
#include <LVUI/Motion.h>
#include <LVUI/StyleTokens.h>
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeOwnedProgressModel final {
    ii::core::Observable<double> minimum{0}, maximum{100}, start{20}, current{80};
    ii::core::Observable<double> width{200}, height{12}, large{8}, regular{4}, radius{1};
    ii::core::Observable<double> displayed_start{.25}, displayed_progress{.5};
    ii::core::Observable<ii::gui::ProgressSize> size{ii::gui::ProgressSize::regular};
    ii::core::Observable<ii::gui::ProgressShape> shape{ii::gui::ProgressShape::round_rect};
    ii::core::Observable<lvui::StyleColor> track{lvui::StyleColor{0, 0, 0, .1}};
    ii::core::Observable<lvui::StyleColor> fill{lvui::StyleColor{0, 1, 0, 1}};
};

struct NativeOwnedProgressSourceModel final {
    static std::optional<ii::gui::ProgressState> initial_state() {
        ii::gui::ProgressState value;
        value.numbers = {{"minimumValue", 0}, {"maximumValue", 100}, {"begin", 10}, {"count", 50}};
        return value;
    }
    ii::core::Observable<std::optional<ii::gui::ProgressState>> state{initial_state()};
    ii::core::Observable<std::string> key{"count"};
    ii::core::Observable<double> fallback{25};
};

struct NativeOwnedProgressMotionModel final {
    ii::core::Observable<double> current{0};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<lvui::Motion> motion{
        lvui::Motion{.duration=std::chrono::milliseconds(400), .curve=lvui::MotionCurve::linear}};
};
