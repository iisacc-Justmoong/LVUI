#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeOpacityModel final {
    static inline int live=0;
    ii::core::Observable<double> alpha{.5},container_alpha{.8};
    ii::core::Observable<bool> shown{true};
    unsigned file_revision=0,commands=0,reads=0;
    NativeOpacityModel(){++live;}~NativeOpacityModel(){--live;}
    double amount(){++reads;return alpha.get();}
    void clicked(){++commands;}
};
