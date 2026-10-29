#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedTooltipModel final {
    ii::core::Observable<std::string> text{std::string("Help")};
    int opened_count=0,closed_count=0;
    void opened(){++opened_count;}
    void closed(){++closed_count;}
};
