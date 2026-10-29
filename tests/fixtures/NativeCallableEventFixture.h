#pragma once
#include <LVRS/ButtonMethod.h>
#include <memory>
struct NativeCallableEventBackend final {
    void accept(const lvrs::ButtonEvent&){}
    void zero(){}
    auto make(){return [value=std::make_unique<int>(1)](const lvrs::ButtonEvent&){(void)value;};}
    auto makeZero(){return [value=std::make_unique<int>(1)]{(void)value;};}
};
