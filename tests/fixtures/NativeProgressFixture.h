#pragma once
#include <LVRS/ProgressBar.h>
#include <LVRS/StateModel.h>
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeProgressBackend final {
    ii::core::Observable<double> minimum{10},maximum{110},start{30},current{80};
    ii::core::Observable<double> width{280},height{9},large{8},regular{4},radius{12},displayed{.4};
    ii::core::Observable<bool> motion_enabled{false};
    ii::core::Observable<lvrs::ProgressSize> size{lvrs::ProgressSize::regular};
    ii::core::Observable<lvrs::ProgressShape> shape{lvrs::ProgressShape::cylinder};
    ii::core::Observable<ii::gui::WidgetColor> track{ii::gui::WidgetColor{0,0,0,.1}},fill{ii::gui::WidgetColor{0,.5,1,1}};
    ii::core::Observable<std::string> low_key{"low"},high_key{"high"},start_key{"begin"},current_key{"done"};
    ii::core::Observable<std::shared_ptr<lvrs::StateModel>> source{std::make_shared<lvrs::StateModel>()};
    std::shared_ptr<lvrs::Motion> motion=std::make_shared<lvrs::Motion>();
};
