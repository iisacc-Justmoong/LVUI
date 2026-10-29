#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <functional>
struct NativeBlueprint final {
    static inline int constructions = 0;
    NativeBlueprint() { ++constructions; }
};
struct NativeFixture final {
    struct Details final { ii::core::Observable<std::string> title{"Detail"}; };
    std::shared_ptr<Details> document = std::make_shared<Details>();
    static inline int constructions = 0, destructions = 0;
    static inline bool fail_construction = false;
    static inline std::function<void()> retire_callback;
    static inline int retirement_returns = 0;
    NativeFixture() {
        if (fail_construction) { fail_construction = false; throw std::runtime_error("Requested native fixture construction failure"); }
        ++constructions;
    }
    ~NativeFixture() { ++destructions; }
    ii::core::Observable<int> count{2}, index{0};
    ii::core::Observable<bool> ready{true};
    ii::core::Observable<std::string> title{"Native"};
    ii::core::Observable<std::vector<double>> rows{std::vector<double>{3, 7}};
    std::string suffix = "caption";
    int calls = 0;
    std::string caption() { ++calls; return suffix; }
    void increment() { count.set(count.get() + 1); }
    void retire() {
        const auto before = destructions;
        if (retire_callback) retire_callback();
        if (destructions != before) throw std::runtime_error("Backend destroyed inside its admitted method");
        ++calls;
        ++retirement_returns;
    }
};
namespace Domain { using Model = NativeFixture; }
