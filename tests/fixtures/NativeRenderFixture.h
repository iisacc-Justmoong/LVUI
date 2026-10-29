#pragma once
#include <LVRS/RenderMonitor.h>
#include <iiSDKCore/Runtime/EventLoop.h>
class NativeRenderBackend final {
public:
    inline static int live=0;
    std::shared_ptr<ii::gui::PresentationStream> source=std::make_shared<ii::gui::PresentationStream>();
    lvrs::RenderMonitor monitor{[]{return std::int64_t(1000);}};
    std::int64_t time=0;
    int file_revision=0;
    NativeRenderBackend(){++live;monitor.attachWindow(source);}
    ~NativeRenderBackend(){--live;}
    void sample(){auto r=source->queued(ii::runtime::Clock::time_point{}+std::chrono::milliseconds(time),ii::gui::GraphicsApi::metal);if(!r)throw std::runtime_error(r.error().message);time+=16;}
    void sample8(){for(int i=0;i<9;++i)sample();}
    void pause(){monitor.stop();}
    void resume(){monitor.start();}
    void reset(){monitor.reset();}
    void replace(){source=std::make_shared<ii::gui::PresentationStream>();time=0;monitor.attachWindow(source);}
    void close(){source->close();}
};
