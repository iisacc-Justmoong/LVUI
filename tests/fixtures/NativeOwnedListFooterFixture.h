#pragma once
#include <LVUI/ListFooter.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <cstddef>
#include <memory>
#include <string>

struct NativeOwnedListFooterModel final {
    ii::core::Observable<std::shared_ptr<const lvui::ListFooterSlot>> first{
        std::make_shared<const lvui::ListFooterSlot>(lvui::ListFooterSlot{.icon_name="home", .accessibility_name="Home"})};
    ii::core::Observable<bool> interactive{true};
    int clicks = 0; std::size_t index = 0; std::string name;
    void clicked(std::size_t value, const lvui::ListFooterSlot& slot) { ++clicks; index = value; name = slot.icon_name; }
};
