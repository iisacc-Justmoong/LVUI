#pragma once
#include <LVRS/ViewModelRegistry.h>
#include <LVRS/StateModel.h>
struct NativeRegistryBackend final {
    lvrs::ViewModelRegistry registry;
    std::shared_ptr<lvrs::StateModel> model=std::make_shared<lvrs::StateModel>();
    ii::core::Observable<int> file_revision{0};
    template<class T>static T take(ii::core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
    NativeRegistryBackend(){
        take(model->set_display_name("Inspector"));take(registry.register_view_model(model,"Canvas"));
        take(registry.bind_view("Editor","Canvas",true));take(registry.bind_view("Preview","Canvas",false));
    }
    void toggle_busy(){take(registry.update_property("Editor","busy",ii::core::Value(!model->busy())));}
    void denied(){take(registry.update_property("Preview","displayName",ii::core::Value("Denied")));}
    void release(){take(registry.release_ownership("Editor"));}
    void replace(){auto next=std::make_shared<lvrs::StateModel>();take(next->set_display_name("Replacement"));take(registry.set("Canvas",next));model=std::move(next);}
};
