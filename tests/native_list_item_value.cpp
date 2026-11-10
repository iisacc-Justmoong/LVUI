#include "NativeListItemValue.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& t,std::string_view suffix){
    auto nodes=take(t->snapshots());auto i=std::ranges::find_if(nodes,[&](const auto& n){return n.id.ends_with(suffix);});
    if(i==nodes.end())throw std::runtime_error("Missing navigation row");return take(t->find(i->id));
}
int main(){
    using namespace ii::gui;using View=lvui_generated::NativeListItemValue;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{400,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));
    auto simple=find(tree,"/simple"),composite=find(tree,"/composite.row");auto graph=scope->native_lifetime.lock();
    check(take(simple.snapshot()).accessibility_description=="Used · 512 GB"&&take(composite.snapshot()).accessibility_description=="Used · 512 GB",
        "Navigation value was not exposed to accessibility");
    good(simple.focus());scope->backend_0.value.set("1 TB");
    check(take(simple.snapshot()).focused&&take(simple.snapshot()).accessibility_description=="Used · 1 TB"&&
          take(composite.snapshot()).accessibility_description=="Used · 1 TB","Value update lost focus or failed to update a row");
    (void)take(tree->drawing_snapshot({400,180}));scope->backend_0.show.set(false);
    check(take(simple.snapshot()).accessibility_description=="Used"&&take(composite.snapshot()).accessibility_description=="Used",
          "Hidden value remained exposed");
    scope->backend_0.value.set(std::string("bad\0data",8));
    check(!take(graph->failures()).empty()&&take(simple.snapshot()).accessibility_description=="Used","Invalid value changed the live row");
    scope->backend_0.value.set("Recovered");scope->backend_0.show.set(true);
    check(take(graph->failures()).empty()&&take(simple.snapshot()).accessibility_description=="Used · Recovered","Navigation value failed to recover");
    good(simple.activate());check(scope->backend_0.activations==1,"Navigation value broke activation");
    good(host.clear_content());scope.reset();graph.reset();check(!simple.snapshot()&&!composite.snapshot(),"Rows survived retirement");tree->close();
}
