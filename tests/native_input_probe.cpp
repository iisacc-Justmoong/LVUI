#include <iiGUI/NativeEventListenerView.h>
#include <LVUI/NativeValues.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiGUI/Host.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
template<class T>T take(ii::core::Result<T> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
    return std::move(*value);
}
void good(ii::core::Result<void> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
}
struct Model final {
    ii::core::Observable<int> x{0},y{0};
    int samples=0,downs=0,moves=0,ups=0;
    bool drawing=false;
    ii::gui::PointerDevice device=ii::gui::PointerDevice::mouse;
};
}

int main(){
    try {
        using namespace ii::gui;
        ii::runtime::EventLoop loop;
        Host host({.title="LVUI Input Probe",.size={640,420},.event_loop=&loop});
        auto model=std::make_shared<Model>();
        auto graph=std::make_shared<lvui::native::Bindings>();
        int shown=0,applications=0;
        take(graph->bind([&](lvui::native::ReadContext& context){return context.read(model->x)+context.read(model->y);},
                         [&](int value)->ii::core::Result<void>{shown=value;++applications;return {};}));
        auto input=lvui::native::scoped_function<ii::core::Result<bool>>(model,graph,
            [](const auto& state,const WidgetInput& event)->ii::core::Result<bool>{
                if(event.kind==WidgetInputKind::pointer_down){state->drawing=true;++state->downs;state->device=event.device;}
                else if(event.kind==WidgetInputKind::pointer_move&&state->drawing)++state->moves;
                else if(event.kind==WidgetInputKind::pointer_up&&state->drawing){state->drawing=false;++state->ups;}
                else return false;
                const auto sample=++state->samples;state->x.set(sample);state->y.set(sample);return true;
            });
        WidgetMount mount;std::optional<ii::gui::NativeEventListener> listener;
        bool timed_out=false,closing=false;
        good(host.run({.opened=[&]{
            mount=take(host.mount_widgets([&](WidgetBuilder& builder)->ii::core::Result<void>{
                auto created=ii::gui::NativeEventListener::create(builder,{},"probe",{640,420},
                    {.input=[&](const WidgetInput& event)->ii::core::Result<bool>{
                        auto result=input(event);
                        if(result&&*result&&event.kind==WidgetInputKind::pointer_up&&!closing){
                            closing=true;
                            if(!host.executor().post([&]{good(host.request_close());}))
                                return ii::core::fail(ii::core::ErrorCode::full,"Could not schedule probe completion");
                        }
                        return result;
                    }});
                if(!created)return std::unexpected(created.error());listener=*created;
                auto label=builder.label(listener->root(),"instructions",{.text="Drag across this window to observe native input.",.font_size=18});
                if(!label)return std::unexpected(label.error());
                return label->set_frame({35,35,570,40});
            }));
            if(!host.executor().after(std::chrono::seconds(90),[&]{timed_out=true;good(host.request_close());}))
                throw std::runtime_error("Could not schedule probe timeout");
            std::cout<<"OS_INPUT_PROBE_READY"<<std::endl;
        }}));
        const auto bindings=take(graph->stats());
        const auto stats=loop.stats();
        std::cout<<"OS_INPUT_TRACE down="<<model->downs<<" moves="<<model->moves<<" up="<<model->ups
                 <<" input="<<model->samples<<" effects="<<applications-1<<" final="<<shown
                 <<" invalidations="<<bindings.invalidations<<" coalesced="<<bindings.coalesced
                 <<" loop_submitted="<<stats.submitted<<" loop_executed="<<stats.executed
                 <<" device="<<unsigned(model->device)<<std::endl;
        if(timed_out||model->downs!=1||model->moves<1||model->ups!=1||
           applications-1!=model->samples||shown!=2*model->samples)
            throw std::runtime_error("Native input did not settle as one binding update per event");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
