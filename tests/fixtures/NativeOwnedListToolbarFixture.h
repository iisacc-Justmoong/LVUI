#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <LVUI/AuthoredIcons.h>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

struct NativeOwnedListToolbarModel final {
    ii::core::Observable<std::string> icon{"home"};
    ii::core::Observable<bool> interactive{true};
    ii::core::Observable<std::shared_ptr<const lvui::IconSource>> source{};
    ii::core::Observable<int> revision{0};
    int clicks = 0; std::size_t index = 0; std::string name;
    void clicked(std::size_t value, std::string source) { ++clicks; index = value; name = std::move(source); }
};
