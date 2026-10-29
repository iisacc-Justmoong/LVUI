#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include "fixtures/observable_module/Model.h"
#include <string>
struct NativeSharedMethodBackend final {
    static inline int live=0;
    ii::core::Observable<int> file_revision{0};
    NativeSharedMethodBackend():model_(observable_model_create()){++live;}
    ~NativeSharedMethodBackend(){observable_model_destroy(model_);--live;}
    NativeSharedMethodBackend(const NativeSharedMethodBackend&)=delete;
    NativeSharedMethodBackend& operator=(const NativeSharedMethodBackend&)=delete;
    std::string summary()const{return std::to_string(observable_model_value(model_));}
    void set(int value){observable_model_set(model_,value);}
private:
    void* model_;
};
