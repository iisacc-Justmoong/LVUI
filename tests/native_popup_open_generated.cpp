#include "NativePopupOpen.generated.h"
#include "NativePopupOpenStatic.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using ii::gui::detail::WidgetState;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
    return std::move(*value);
}
void good(ii::core::Result<void> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
}
ii::gui::Widget find(const std::shared_ptr<WidgetState>& state,std::string_view id){
    const auto nodes=take(state->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& item){return item.id==id||item.id.ends_with(std::string("/")+std::string(id));});
    if(found==nodes.end())throw std::runtime_error("Missing popup: "+std::string(id));
    return take(state->find(found->id));
}
}

int main(){
    using View=lvui_generated::NativePopupOpen;
    using Static=lvui_generated::NativePopupOpenStatic;
    auto tree=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{800,600}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto modal=find(tree,"modal"),alert=find(tree,"alert"),sheet=find(tree,"sheet"),blocked=find(tree,"blocked"),scoped=find(tree,"scoped");
    auto nested=take(modal.popup_content("nested"));
    auto nested_body=take(nested.popup_content("nestedBody"));
    auto draft=take(modal.popup_content("draft"));
    const auto draft_id=take(draft.snapshot()).id,nested_id=take(nested_body.snapshot()).id;
    require(take(modal.snapshot()).opened&&take(nested.snapshot()).opened&&
            scope->backend_0.modalOpens==1&&scope->backend_0.nestedOpens==1,
            "Reactive initially open nested modals did not publish");
    auto graph=scope->native_lifetime.lock();require(bool(graph)&&take(graph->failures()).empty(),"Popup bindings are unavailable");
    scope->backend_0.title.set("Changed");
    require(take(draft.snapshot()).id==draft_id&&take(nested_body.snapshot()).id==nested_id&&
            scope->backend_0.modalOpens==1&&scope->backend_0.nestedOpens==1,
            "Unrelated updates recreated an open modal or nested content");
    scope->backend_0.nestedOpen.set(false);
    require(!take(nested.snapshot()).opened&&!nested_body.snapshot()&&take(modal.snapshot()).opened,
            "Reactive nested close did not retire only its own body");
    scope->backend_0.nestedOpen.set(true);
    require(take(nested.snapshot()).opened&&take(take(nested.popup_content("nestedBody")).snapshot()).id!=nested_id&&
            scope->backend_0.nestedOpens==2,"Reactive nested reopen did not create a new body");
    scope->backend_0.alertOpen.set(true);
    require(take(alert.snapshot()).opened&&scope->backend_0.alertOpens==1,"Reactive alert did not open");
    good(alert.cancel_popup());
    require(!take(alert.snapshot()).opened&&scope->backend_0.alertOpen.get()&&scope->backend_0.alertCloses==1,
            "Direct dismissal changed the source value or reopened the alert");
    scope->backend_0.alertOpen.set(false);
    require(!take(alert.snapshot()).opened&&scope->backend_0.alertCloses==1,"Source acknowledgement closed the alert twice");
    scope->backend_0.alertOpen.set(true);scope->backend_0.alertOpen.set(false);
    require(!take(alert.snapshot()).opened&&scope->backend_0.alertOpens==2&&scope->backend_0.alertCloses==2,
            "Reactive alert did not reopen and close after dismissal");
    scope->backend_0.blockedOpen.set(true);
    require(!take(blocked.snapshot()).opened&&!take(graph->failures()).empty(),"Hidden popup failure was not reported");
    scope->backend_0.blockedVisible.set(true);
    require(take(blocked.snapshot()).opened&&take(graph->failures()).empty(),"Popup did not recover when it became visible");
    scope->backend_0.blockedOpen.set(false);
    scope->backend_0.scopedOpen.set(true);
    require(take(scoped.snapshot()).opened,"A lexical child scope did not apply its popup binding");
    scope->backend_0.scopedOpen.set(false);
    require(!take(scoped.snapshot()).opened,"A lexical child scope did not close its popup");
    scope->backend_0.sheetOpen.set(true);
    auto sheet_input=take(sheet.popup_content("sheetInput"));const auto sheet_id=take(sheet_input.snapshot()).id;
    scope->backend_0.sheetOpen.set(false);scope->backend_0.sheetOpen.set(true);
    require(take(sheet.snapshot()).opened&&take(take(sheet.popup_content("sheetInput")).snapshot()).id==sheet_id&&
            scope->backend_0.sheetOpens==2&&scope->backend_0.sheetCloses==1,
            "Reactive sheet did not preserve its retained content");
    scope->backend_0.showConditional.set(true);
    auto conditional=find(tree,"conditional");
    require(take(conditional.snapshot()).opened&&scope->backend_0.conditionalOpens==1,
            "Initially open conditional alert did not open after publication");
    scope->backend_0.showConditional.set(false);
    require(!conditional.snapshot(),"Conditional alert remained alive after removal");

    std::shared_ptr<View::Scope> rejected_scope;
    auto rejected=host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto built=View::build(builder,{}, {},&rejected_scope);if(!built)return built;
        return ii::core::fail(ii::core::ErrorCode::failed,"Reject candidate");
    });
    require(!rejected&&rejected_scope&&rejected_scope->backend_0.modalOpens==0&&
            take(modal.snapshot()).opened&&scope->backend_0.modalOpens==1,
            "A rejected generation opened a popup or disturbed the live one");
    rejected_scope.reset();
    scope->backend_0.modalOpen.set(false);
    require(!take(modal.snapshot()).opened&&!draft.snapshot(),"Reactive modal did not retire its body");
    scope->backend_0.closeOnOpen.set(true);scope->backend_0.modalOpen.set(true);
    require(!take(modal.snapshot()).opened&&!scope->backend_0.modalOpen.get()&&
            scope->backend_0.modalOpens==2&&scope->backend_0.modalCloses==2&&take(graph->failures()).empty(),
            "Reentrant open callback did not settle the latest state");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!modal.snapshot(),"Popup binding retained a retired activation");

    good(host.set_content([](auto& builder){return Static::build(builder);}));
    auto static_alert=find(tree,"staticAlert"),static_modal=find(tree,"staticModal"),static_sheet=find(tree,"staticSheet");
    require(take(static_alert.snapshot()).opened&&take(static_modal.snapshot()).opened&&take(static_sheet.snapshot()).opened&&
            take(take(static_modal.popup_content("staticBody")).snapshot()).text=="Static"&&
            take(take(static_sheet.popup_content("staticSheetBody")).snapshot()).text=="Sheet",
            "Statically open popups did not open in an embedded view");
    good(host.clear_content());
    std::shared_ptr<View::Scope> invalid_scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto& value){
        value.backend_0.modalOpen.set(false);value.backend_0.blockedOpen.set(true);
    },&invalid_scope);}));
    auto invalid_graph=invalid_scope->native_lifetime.lock();require(bool(invalid_graph),"Initial failure graph was not retained");
    const auto failures=take(invalid_graph->failures());bool tracked=false;
    for(const auto& [weak_graph,id]:invalid_scope->initial_popup_bindings)
        if(weak_graph.lock()==invalid_graph)for(const auto& failure:failures)tracked|=failure.binding==id;
    require(!failures.empty()&&tracked&&!take(find(tree,"blocked").snapshot()).opened,
            "Initial publication failure was not available to mount validation");
    invalid_scope->backend_0.blockedVisible.set(true);
    require(take(find(tree,"blocked").snapshot()).opened&&take(invalid_graph->failures()).empty(),
            "Initial publication failure did not recover");
    good(host.clear_content());invalid_scope.reset();invalid_graph.reset();tree->close();
}
