#pragma once
#include <LVUI/AuthoredIcons.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

struct NativeOwnedIconModel final {
    ii::core::Observable<std::string> name{std::string("brush")};
    ii::core::Observable<double> size{18};
    ii::core::Observable<std::shared_ptr<const lvui::IconSource>> source{};
    ii::core::Observable<int> revision{0};
    ii::core::Observable<std::string> label{std::string("Brush")};
};
