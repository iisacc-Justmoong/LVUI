#pragma once
#include <LVRS/PageRouter.h>
#include <LVRS/StateModel.h>
#include <LVRS/ViewModelRegistry.h>
struct NativeRouteRegistryBackend final {
    using V=ii::core::Value;
    std::shared_ptr<lvrs::ViewModelRegistry> registry=std::make_shared<lvrs::ViewModelRegistry>();
    std::shared_ptr<lvrs::StateModel> model=std::make_shared<lvrs::StateModel>();
    std::shared_ptr<lvrs::PageRouter> router;
    ii::core::Observable<int> file_revision{0};
    template<class T>static T take(ii::core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
    NativeRouteRegistryBackend(){
        take(model->set_display_name("Canvas"));take(registry->set("Canvas",model));
        auto page=std::make_shared<const lvrs::PageComponent>([](ii::gui::WidgetBuilder& widgets,ii::gui::Widget parent,const lvrs::NavigationEntry& entry)->ii::core::Result<void>{auto node=widgets.label(parent,"page",{.text=entry.view_id});if(!node)return std::unexpected(node.error());return {};});
        auto broken=std::make_shared<const lvrs::PageComponent>([](ii::gui::WidgetBuilder&,ii::gui::Widget,const auto&)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"Broken compiled page");});
        router=take(lvrs::PageRouter::create({.routes={{"/",page,{{"modelKey",V("Canvas")},{"viewId",V("Preview")}}},{"/editor",page,{{"modelKey",V("Canvas")},{"viewId",V("Editor")},{"writable",V(true)}}},{"/broken",broken,{{"modelKey",V("Canvas")},{"viewId",V("Broken")},{"writable",V(true)}}}},.register_as_global_navigator=false,.view_models=registry}));
    }
    std::optional<V> name()const{return registry->read_property("Editor","displayName");}
    bool can_edit()const{return registry->can_write("Editor");}
    std::size_t bound_views()const{return registry->views().size();}
    void edit(){take(router->go("/editor"));}
    void back(){take(router->back());}
    void rename(){take(registry->update_property("Editor","displayName",V("Renamed")));}
    void failed_page(){auto result=router->replace("/broken");if(result)throw std::runtime_error("Broken page was published");}
};
