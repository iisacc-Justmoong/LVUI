#include <LVUI/NativePageRouter.h>
#include "NativeOwnedPageRouter.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Page final:lvui::native::NativeNavigationPage {
    std::string label;bool fail=false;mutable bool has_target=false,reentry_rejected=false;
    lvui::PageRouter* router=nullptr;
    explicit Page(std::string text):label(std::move(text)){}
    ii::core::Result<void> build(ii::gui::WidgetBuilder& builder,const lvui::NavigationEntry&,
            const std::shared_ptr<lvui::NavigationContext>& context)const override {
        has_target=bool(context->target());if(router){auto nested=router->back();reentry_rejected=!nested&&nested.error().code==ii::core::ErrorCode::busy;}
        auto root=builder.label({},"page",{.text=label});if(!root)return std::unexpected(root.error());
        if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"Page build failed");return {};
    }
};
struct PlainPage final:lvui::NavigationPage {};
struct OtherTarget final:lvui::NavigationTarget {
    ii::core::Result<bool> navigate(const lvui::NavigationRequest&)override{return true;}
};
void hosts(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));
    auto home=std::make_shared<Page>("Home"),report=std::make_shared<Page>("Report"),bad=std::make_shared<Page>("Bad");bad->fail=true;
    auto plain=std::make_shared<PlainPage>();auto router=take(lvui::PageRouter::create({{"/",home},{"/report/[id]",report},{"/bad",bad},{"/plain",plain}}));
    home->router=router.get();report->router=router.get();
    auto context=std::make_shared<lvui::NavigationContext>();std::optional<lvui::native::NativePageRouter> first,second;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{auto view=lvui::native::NativePageRouter::create(builder,{},"router",router,context,{640,480});
        if(!view)return std::unexpected(view.error());first=*view;return {};}));
    require(context->target()==router&&take(router->state()).presenter_attached,"Published host did not bind its navigation target");
    require(take(router->set_root("/"))&&home->has_target&&home->reentry_rejected,"Initial page or scoped context failed");
    auto old_page=take(first->root().content_item("page"));require(take(old_page.snapshot()).text=="Home","Home page was not published");
    require(take(router->navigate({.href="/report/42"}))&&report->has_target&&report->reentry_rejected&&!old_page.snapshot(),"Report did not replace and retire Home");
    auto visible=take(first->root().content_item("page"));const auto before=take(router->state()).history;
    auto failed=router->navigate({.href="/bad"});require(!failed&&visible.snapshot()&&take(router->state()).history.revision==before.revision,
        "A failed page build changed the visible page or history");
    failed=router->navigate({.href="/plain"});require(!failed&&visible.snapshot()&&take(router->state()).history.revision==before.revision,
        "A page without a native implementation changed the view or history");
    auto rejected=outer.set_content([&](auto& builder)->ii::core::Result<void>{auto candidate=lvui::native::NativePageRouter::create(builder,{},"candidate",router,context,{500,300});
        if(!candidate)return std::unexpected(candidate.error());return ii::core::fail(ii::core::ErrorCode::failed,"Reject outer candidate");});
    require(!rejected&&visible.snapshot()&&context->target()==router&&take(router->state()).presenter_attached,
        "Rejected outer host disturbed the active router");
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{auto candidate=lvui::native::NativePageRouter::create(builder,{},"replacement",router,context,{500,300});
        if(!candidate)return std::unexpected(candidate.error());second=*candidate;return {};}));
    require(!visible.snapshot()&&take(second->root().content_item("page").value().snapshot()).text=="Report"&&
        take(router->state()).history.revision==before.revision&&context->target()==router,
        "Host replacement lost the current page or changed history");
    require(take(router->back())&&take(second->root().content_item("page").value().snapshot()).text=="Home","Back did not rebuild the previous page");
    good(second->root().set_extent({320,240}));auto replacement=std::make_shared<OtherTarget>();context->set_target(replacement);
    good(outer.clear_content());
    require(context->target()==replacement&&!take(router->state()).presenter_attached&&!router->navigate({.href="/report/1"}),
        "Retired host disturbed a newer context target or remained attached to navigation");
    std::weak_ptr<lvui::PageRouter> weak_router=router;router.reset();
    require(weak_router.expired(),"Retired host handles retained the page router");tree->close();
}
void delayed_publication(){
    bool delay=false;std::vector<ii::runtime::Task> pending;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([&](ii::runtime::Task task){if(delay)pending.push_back(std::move(task));else task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));auto router=take(lvui::PageRouter::create({}));
    auto context=std::make_shared<lvui::NavigationContext>();std::optional<lvui::native::NativePageRouter> view;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{auto candidate=lvui::native::NativePageRouter::create(builder,{},"router",router,context);
        if(!candidate)return std::unexpected(candidate.error());view=*candidate;delay=true;return {};}));
    require(!pending.empty(),"Publication callback was not delayed");good(outer.clear_content());
    delay=false;for(auto& task:pending)task();
    require(!context->target()&&!take(router->state()).presenter_attached&&!view->root().snapshot(),
        "Late publication reattached a retired host");tree->close();
}
void generated(){
    using View=lvui_generated::NativeOwnedPageRouter;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    const auto find=[&](std::string_view suffix){auto items=take(tree->snapshots());auto found=std::ranges::find_if(items,[&](const auto& item){return item.id.ends_with(suffix);});
        if(found==items.end())throw std::runtime_error("Generated router widget missing");return take(tree->find(found->id));};
    auto link=find("/report-link"),pages=find("/pages");auto router=scope->backend_0.router;
    require(scope->navigation_context->target()==router&&take(router->set_root("/")),"Generated page router did not attach");
    require(take(take(pages.content_item("home")).snapshot()).text=="Home","Generated router lost the root page");
    good(link.activate());require(take(take(pages.content_item("report")).snapshot()).text=="Report 42"&&
        take(router->state()).history.depth==2,"Generated Link did not navigate through the independent router");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated router binding graph missing");
    scope->backend_0.width.set(320);scope->backend_0.height.set(180);require(take(graph->failures()).empty(),"Valid router dimensions were rejected");
    scope->backend_0.width.set(-1);require(!take(graph->failures()).empty()&&take(router->state()).history.depth==2,
        "Invalid dimensions were accepted or changed navigation history");
    scope->backend_0.width.set(360);require(take(graph->failures()).empty(),"Router dimensions did not recover");
    good(outer.clear_content());require(!router->navigate({.href="/"}),"Generated router remained connected after retirement");
    tree->close();
}
}
int main(){hosts();delayed_publication();generated();}
