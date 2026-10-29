#pragma once
#include "PageRouter.h"
#include <iiGUI/NativeWidgets.h>
#include <exception>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace lvui::native {
// Applications implement this interface to construct a page using the generic
// iiGUI widget host. A page receives its route values and a navigation context.
class NativeNavigationPage:public NavigationPage {
public:
    [[nodiscard]] virtual ii::core::Result<void> build(ii::gui::WidgetBuilder&,
        const NavigationEntry&,const std::shared_ptr<NavigationContext>&)const=0;
};

using NativePageFactory=std::function<ii::core::Result<void>(ii::gui::WidgetBuilder&,
    const NavigationEntry&,const std::shared_ptr<NavigationContext>&)>;
class FunctionalNavigationPage final:public NativeNavigationPage {
    NativePageFactory factory_;
public:
    explicit FunctionalNavigationPage(NativePageFactory factory):factory_(std::move(factory)){}
    [[nodiscard]] ii::core::Result<void> build(ii::gui::WidgetBuilder& builder,const NavigationEntry& entry,
            const std::shared_ptr<NavigationContext>& context)const override {
        if(!factory_)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Page factory is unavailable");
        return factory_(builder,entry,context);
    }
};

namespace detail {
struct PagePresentation final {
    ii::gui::Widget root;
    std::shared_ptr<NavigationContext> context;
    bool building=true,live=false;
};
class PagePresenter final:public NavigationPresenter {
    std::weak_ptr<PagePresentation> presentation_;
public:
    explicit PagePresenter(const std::shared_ptr<PagePresentation>& presentation):presentation_(presentation){}
    [[nodiscard]] ii::core::Result<void> present(const NavigationEntry& entry,
            std::function<ii::core::Result<bool>()> commit)override {
        auto state=presentation_.lock();
        if(!state||(!state->building&&!state->live))return ii::core::fail(ii::core::ErrorCode::closed,"Page host has retired");
        auto page=std::dynamic_pointer_cast<const NativeNavigationPage>(entry.page);
        if(!page)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Page does not implement the LVUI native page interface");
        return state->root.set_content([page,entry,context=state->context](ii::gui::WidgetBuilder& builder){
            return page->build(builder,entry,context);
        },[commit=std::move(commit)]()->ii::core::Result<void>{
            auto result=commit();if(!result)return std::unexpected(result.error());
            if(!*result)return ii::core::fail(ii::core::ErrorCode::busy,"Page history was not committed");return {};
        });
    }
};
} // namespace detail

class NativePageRouter final {
    struct State final {
        std::shared_ptr<PageRouter> router;
        std::shared_ptr<NavigationContext> context;
        std::shared_ptr<detail::PagePresentation> presentation;
        std::shared_ptr<detail::PagePresenter> presenter;
        bool retired=false;
    };
    std::shared_ptr<State> state_;
    explicit NativePageRouter(std::shared_ptr<State> state):state_(std::move(state)){}
public:
    [[nodiscard]] static ii::core::Result<NativePageRouter> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,std::shared_ptr<PageRouter> router,std::shared_ptr<NavigationContext> context,
            ii::gui::Size size={640,400}){
        if(!router||!context)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Page router and context are required");
        try {
            auto state=std::make_shared<State>();state->router=std::move(router);state->context=std::move(context);
            state->presentation=std::make_shared<detail::PagePresentation>();
            state->presentation->context=std::make_shared<NavigationContext>();state->presentation->context->set_target(state->router);
            auto root=builder.content_host(parent,id,size);if(!root)return std::unexpected(root.error());state->presentation->root=*root;
            state->presenter=std::make_shared<detail::PagePresenter>(state->presentation);
            if(auto owned=builder.retain(state);!owned)return std::unexpected(owned.error());
            // Prepare any existing page while the new host is still a candidate.
            // Failure leaves both the published host and its presenter intact.
            auto synchronized=state->router->synchronize(state->presenter);if(!synchronized)return std::unexpected(synchronized.error());
            state->presentation->building=false;
            std::weak_ptr<State> weak=state;
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired){
                state->presentation->live=true;
                auto attached=state->router->set_presenter(state->presenter);
                if(attached)state->context->bind_target(state->router,state);
                else {state->presentation->live=false;state->presenter.reset();}
            }});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;
                state->presentation->live=false;state->presentation->building=false;
                state->context->unbind_target(state);state->presenter.reset();state->router.reset();state->context.reset();
            }});!registered)return std::unexpected(registered.error());
            return NativePageRouter(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page host construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->presentation->root;}
};
} // namespace lvui::native
