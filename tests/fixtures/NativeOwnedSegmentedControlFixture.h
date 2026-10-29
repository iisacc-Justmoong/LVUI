#pragma once
#include <LVUI/Button.h>
#include <LVUI/SegmentedControl.h>
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <vector>
struct NativeOwnedSegmentedItem final {
    std::string id,label;
    friend bool operator==(const NativeOwnedSegmentedItem&,const NativeOwnedSegmentedItem&)=default;
};
struct NativeOwnedSegmentedControlModel final {
    ii::core::Observable<bool> force{true},enabled{true},show{true},innerForce{false};
    ii::core::Observable<double> spacing{2};
    ii::core::Observable<lvui::SegmentedShape> shape{lvui::SegmentedShape::round_rect};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{};
    ii::core::Observable<std::vector<NativeOwnedSegmentedItem>> items{std::vector<NativeOwnedSegmentedItem>{{"a","Repeated"}}};
    unsigned clicks=0;
    void clicked(){++clicks;}
};
