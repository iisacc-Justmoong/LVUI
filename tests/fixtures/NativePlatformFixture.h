#pragma once
#include <LVRS/PlatformInfo.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
struct NativePlatformBackend final {
    static inline int live=0;
    std::shared_ptr<const lvrs::PlatformInfo> mac=std::make_shared<lvrs::PlatformInfo>(lvrs::PlatformEnvironment{"macos","arm64",true,true});
    std::shared_ptr<const lvrs::PlatformInfo> phone=std::make_shared<lvrs::PlatformInfo>(lvrs::PlatformEnvironment{"ios","arm64",true,false});
    ii::core::Observable<std::shared_ptr<const lvrs::PlatformInfo>> selected{mac};
    unsigned file_revision=0;
    NativePlatformBackend(){++live;}~NativePlatformBackend(){--live;}
    void selectPhone(){selected.set(phone);}
    void selectMac(){selected.set(mac);}
};
