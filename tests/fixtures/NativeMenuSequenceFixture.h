#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeMenuSequenceModel final {
    ii::core::Observable<std::string> first_label{"First action"};
    ii::core::Observable<std::string> second_label{"Second action"};
    ii::core::Observable<bool> show_second{true};
    int first_calls=0,second_calls=0;
    std::vector<int> triggered;
    void first(){++first_calls;}
    void second(){++second_calls;}
    void on_triggered(int index){triggered.push_back(index);}
};
