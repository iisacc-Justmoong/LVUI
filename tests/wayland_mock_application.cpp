#include "LVUINativeMultiWindowFixture.generated.h"
#include "LVUINativeWindowCloseFixture.generated.h"
#include "LVUINativeForegroundFixture.generated.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

void fake_wayland_clear_wakes();
void fake_wayland_reset_waits();
void fake_wayland_request_all_close();
namespace {
using View=lvui_generated::LVUINativeMultiWindowFixture;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
struct Cleanup final {~Cleanup(){NativeApplicationBackend::during_ready={};WindowCloseFixtureBackend::during_open={};WindowCloseFixtureBackend::during_request={};fake_wayland_clear_wakes();}};
void application_owns_multiple_wayland_scenes(){
    Cleanup cleanup;fake_wayland_reset_waits();
    NativeApplicationBackend::events.clear();NativeWindowBackend::scene_events.clear();
    const int apps=NativeApplicationBackend::created, windows=NativeWindowBackend::created;
    NativeApplicationBackend::during_ready=[] {fake_wayland_request_all_close();};
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(View::configure_application(app));
    auto outcome=app.exec();
    if(!outcome){if(outcome.error().cause)std::rethrow_exception(outcome.error().cause);throw std::runtime_error(outcome.error().message);}
    check(*outcome==0,"Generated application returned a nonzero result");
    check(NativeApplicationBackend::created==apps+1 && NativeWindowBackend::created==windows+2,
          "Generated application did not create one app and two window scopes");
    check(NativeApplicationBackend::events==std::vector<std::string>({"start","ready","stop"}) &&
          NativeWindowBackend::scene_events.size()==4,
          "Generated application or window lifecycle was not delivered and retired");
    check(NativeApplicationBackend::stopped_after_window,"Application stopped before its windows retired");
}
void window_veto_and_retry_share_the_wayland_driver(){
    Cleanup cleanup;fake_wayland_reset_waits();
    using CloseView=lvui_generated::LVUINativeWindowCloseFixture;
    using Backend=WindowCloseFixtureBackend;
    Backend::created=Backend::destroyed=Backend::opened_count=0;Backend::requests.clear();Backend::allow.fill(true);Backend::fail=false;
    Backend::allow[0]=false;NativeApplicationBackend::events.clear();NativeApplicationBackend::allow_close_request=true;
    Backend::during_open=[] {if(Backend::opened_count==2)fake_wayland_request_all_close();};
    Backend::during_request=[](int index){if(index==0&&!Backend::allow[0]){Backend::allow[0]=true;fake_wayland_request_all_close();}};
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(CloseView::configure_application(app));
    auto outcome=app.exec();
    if(!outcome){if(outcome.error().cause)std::rethrow_exception(outcome.error().cause);throw std::runtime_error(outcome.error().message);}
    check(*outcome==0 && Backend::created==2 && Backend::destroyed==2 && Backend::requests==std::vector<int>({0,1,0}) &&
          NativeApplicationBackend::events==std::vector<std::string>{"close-request"},
          "Window veto, retry, or final app approval was lost through the Wayland host");
}
void unsupported_foreground_rolls_back_application_configuration(){
    using Foreground=lvui_generated::LVUINativeForegroundFixture;
    check(ii::gui::host_capabilities().backend==ii::gui::HostBackend::wayland &&
          !ii::gui::supports_application_foreground(),"The mock Wayland capability is incorrect");
    ii::runtime::Application app;
    auto result=Foreground::configure_application(app);
    check(!result && result.error().code==ii::core::ErrorCode::unsupported,
          "Wayland silently accepted unsupported foreground delivery");
    good(app.set_driver({[](const auto& start){return start();},[]{return ii::core::Result<void>{};}}));
    check(bool(app.exec()),"Unsupported foreground left the application partially configured");
}
}
int main(){try{application_owns_multiple_wayland_scenes();window_veto_and_retry_share_the_wayland_driver();unsupported_foreground_rolls_back_application_configuration();}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
