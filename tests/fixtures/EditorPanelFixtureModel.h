#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

// App-owned test model. The same explicit event endpoint accepts each typed
// control notification; the fixture does not access a platform or a document.
struct EditorPanelFixtureModel final {
    ii::core::Observable<std::string> text{"Draft"};
    ii::core::Observable<bool> enabled{true}, checked{true};
    ii::core::Observable<double> amount{0.5}, minimum{0.0}, maximum{1.0};
    ii::core::Observable<int> index{0};
    ii::core::Observable<std::vector<std::string>> choices{std::vector<std::string>{"One", "Two"}};
    int events = 0;

    template<class... Args> void execute(Args&&...) { ++events; }
};
