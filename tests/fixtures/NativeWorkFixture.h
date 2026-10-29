#pragma once
#include <LVUI/NativeWork.h>
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeWorkBackend final {
    static inline int constructions=0,destructions=0;
    static inline bool wrong_thread=false;
    static inline std::function<void(std::stop_token)> body;
    static inline std::function<void()> during_reply;
    std::shared_ptr<lvui::native::WorkScope> work;
    std::thread::id owner=std::this_thread::get_id();
    int result=0,starts=0;
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<int> file_revision{0};
    explicit NativeWorkBackend(std::shared_ptr<lvui::native::WorkScope> work):work(std::move(work)){if(!this->work)throw std::logic_error("Missing activation work");++constructions;}
    NativeWorkBackend(const NativeWorkBackend&)=delete;NativeWorkBackend(NativeWorkBackend&&)=delete;
    ~NativeWorkBackend(){if(owner!=std::this_thread::get_id())wrong_thread=true;++destructions;}
    std::string error()const{auto failed=work->failure();return failed?failed->message:"";}
    void start(){
        ++starts;
        auto submitted=work->post([this,body=body,reply=during_reply](std::stop_token stop){
            if(body)body(stop);if(stop.stop_requested())return;
            (void)work->deliver([this,reply,value=std::make_unique<int>(42)]{if(reply)reply();result=*value;});
        });
        if(!submitted)throw std::runtime_error("Native work admission failed");
    }
};
