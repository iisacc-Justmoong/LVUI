#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <memory>
#include <stdexcept>
#include <string>
struct NativeConstructionResource final {
    int value=0;
    static inline int destructions=0;
    ~NativeConstructionResource(){++destructions;}
};
struct NativeConstructedBackend final {
    static inline int constructions=0,destructions=0,bad_resource_order=0;
    explicit NativeConstructedBackend(const std::shared_ptr<NativeConstructionResource>& resource):NativeConstructedBackend(resource,resource->value){}
    NativeConstructedBackend(const std::shared_ptr<NativeConstructionResource>& resource,int initial):
        resource(resource),value(initial),title("Injected"){++constructions;}
    NativeConstructedBackend(ii::runtime::Runtime& executor,const std::shared_ptr<NativeConstructionResource>& resource):
        NativeConstructedBackend(resource,int(executor.stats().workers)){runtime=&executor;}
    NativeConstructedBackend(const NativeConstructedBackend&)=delete;
    NativeConstructedBackend(NativeConstructedBackend&&)=delete;
    ~NativeConstructedBackend(){if(resource.expired())++bad_resource_order;++destructions;}
    std::weak_ptr<NativeConstructionResource> resource;
    ii::runtime::Runtime* runtime=nullptr;
    ii::core::Observable<int> value;
    ii::core::Observable<std::string> title;
    ii::core::Observable<bool> visible{true};
    void increment(){value.set(value.get()+1);}
    ii::runtime::Submission submit(ii::runtime::Task task){return runtime?runtime->post(std::move(task)):ii::runtime::Submission{{},ii::runtime::SubmitError::invalid};}
};
struct NativeConstructedLater final {
    static inline bool fail=false;
    NativeConstructedLater(){if(fail)throw std::runtime_error("Requested later construction failure");}
};
