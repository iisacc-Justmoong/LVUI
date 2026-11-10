#pragma once
#include <iiGUI/NativeWheelScrollGuard.h>
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeWheelGuardModel final {
    ii::core::Observable<bool> enabled{true},consume{true};
    int events=0;
    void routed(const ii::gui::WheelScrollObservation&){++events;}
};
