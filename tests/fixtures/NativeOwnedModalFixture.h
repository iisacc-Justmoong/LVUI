#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedModalModel final {
    ii::core::Observable<std::string> title{"Initial"}, description{"Details"}, text{"Body"};
    ii::core::Observable<double> width{600};
    ii::core::Observable<bool> cancelEnabled{true};
    int saves = 0, opens = 0, closes = 0, cancels = 0, edits = 0;
    void saved() { ++saves; }
    void opened() { ++opens; }
    void closed() { ++closes; }
    void canceled() { ++cancels; }
    void edited(std::string) { ++edits; }
};
