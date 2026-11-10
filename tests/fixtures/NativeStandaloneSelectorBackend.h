#pragma once
#include <LVUI/ListItemSelector.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>

struct NativeStandaloneSelectorBackend final {
    ii::core::Observable<std::vector<lvui::ListItemOption>> options{{{"Draft",true},{"Unavailable",false},{"Published",true}}};
    ii::core::Observable<int> index{0};
    ii::core::Observable<bool> enabled{true};
    std::vector<int> events;
    void changed(int value){events.push_back(value);index.set(value);}
};
