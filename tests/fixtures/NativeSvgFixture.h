#pragma once
#include <LVRS/SvgManager.h>
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeSvgRequest final:lvrs::SvgRequest {
    lvrs::SvgCompletion completion;bool cancelled=false;
    explicit NativeSvgRequest(lvrs::SvgCompletion value):completion(std::move(value)){}
    void cancel()noexcept override{cancelled=true;completion={};}
};
struct NativeSvgSource final:lvrs::SvgSource {
    std::shared_ptr<NativeSvgRequest> pending;unsigned requests=0;
    double device_scale()const override{return 2;}
    ii::core::Result<lvrs::SvgResource> read(std::string_view url)override{return lvrs::SvgResource{std::string(url),std::nullopt};}
    ii::core::Result<std::shared_ptr<lvrs::SvgRequest>> fetch(std::string_view,lvrs::SvgCompletion completion)override{
        pending=std::make_shared<NativeSvgRequest>(std::move(completion));++requests;return std::shared_ptr<lvrs::SvgRequest>(pending);
    }
};
struct NativeSvgBackend final {
    static inline int live=0;
    std::shared_ptr<NativeSvgSource> source=std::make_shared<NativeSvgSource>();
    lvrs::SvgManager policy{source};
    ii::core::Observable<double> width{64},height{64},padding{0};
    ii::core::Observable<std::string> selected{"https://fixture/first.svg"};
    unsigned file_revision=0;
    NativeSvgBackend(){++live;}~NativeSvgBackend(){--live;}
    std::string url()const{return selected.get();}
    void complete(){if(source->pending&&source->pending->completion){auto callback=std::move(source->pending->completion);callback(std::string(R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8" fill="red"/></svg>)"));}}
    void fail(){if(source->pending&&source->pending->completion){auto callback=std::move(source->pending->completion);callback(ii::core::fail(ii::core::ErrorCode::failed,"Fixture SVG failure"));}}
    void resize(){width.set(120);height.set(72);}
    void retarget(){selected.set("https://fixture/second.svg");}
};
