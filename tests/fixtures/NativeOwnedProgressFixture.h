#pragma once
#include <LVUI/ProgressBar.h>
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeOwnedProgressModel final {
    ii::core::Observable<double> minimum{0}, maximum{100}, start{20}, current{80};
    ii::core::Observable<double> width{200}, height{12}, large{8}, regular{4}, radius{1};
    ii::core::Observable<double> displayed_start{.25}, displayed_progress{.5};
    ii::core::Observable<lvui::ProgressSize> size{lvui::ProgressSize::regular};
    ii::core::Observable<lvui::ProgressShape> shape{lvui::ProgressShape::round_rect};
    ii::core::Observable<lvui::StyleColor> track{lvui::StyleColor{0, 0, 0, .1}};
    ii::core::Observable<lvui::StyleColor> fill{lvui::StyleColor{0, 1, 0, 1}};
};

struct NativeOwnedProgressSourceModel final {
    static std::optional<lvui::ProgressState> initial_state() {
        lvui::ProgressState value;
        value.numbers = {{"minimumValue", 0}, {"maximumValue", 100}, {"begin", 10}, {"count", 50}};
        return value;
    }
    ii::core::Observable<std::optional<lvui::ProgressState>> state{initial_state()};
    ii::core::Observable<std::string> key{"count"};
    ii::core::Observable<double> fallback{25};
};

struct NativeOwnedProgressMotionModel final {
    ii::core::Observable<double> current{0};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<lvui::Motion> motion{
        lvui::Motion{.duration=std::chrono::milliseconds(400), .curve=lvui::MotionCurve::linear}};
};
