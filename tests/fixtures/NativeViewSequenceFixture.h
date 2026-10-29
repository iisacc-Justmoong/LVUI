#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
struct NativeSequenceModel final {
    static inline int constructions=0,destructions=0;
    ii::core::Observable<std::string> title{"Native"};
    ii::core::Observable<int> count{2},file_revision{0};
    ii::core::Observable<bool> ready{true};
    ii::core::Observable<std::vector<std::string>> items{std::vector<std::string>{"a","b"}};
    int factories=0;
    NativeSequenceModel(){++constructions;}
    ~NativeSequenceModel(){++destructions;}
    void increment(){count.set(count.get()+1);}
    auto makeHandler(){++factories;return [receiver=this,state=std::make_unique<int>(0)]{++*state;receiver->increment();};}
};
struct NativeSequenceLocal final {
    static inline int attempts=0,constructions=0,destructions=0,fail_at=0,retirement_returns=0;
    static inline std::set<int> live;
    static inline std::function<void()> retirement;
    ii::core::Observable<std::string> title{"Local"};
    ii::core::Observable<int> count{100};
    int identity=0;
    NativeSequenceLocal(){if(++attempts==fail_at)throw std::runtime_error("Sequence local construction failed");identity=++constructions;live.insert(identity);}
    ~NativeSequenceLocal(){live.erase(identity);++destructions;}
    void increment(){count.set(count.get()+1);}
    void retire(){auto function=retirement;if(function)function();if(!live.contains(identity))throw std::runtime_error("An active sequence receiver was destroyed");++retirement_returns;}
    static void reset(){attempts=constructions=destructions=fail_at=retirement_returns=0;live.clear();retirement={};}
};
