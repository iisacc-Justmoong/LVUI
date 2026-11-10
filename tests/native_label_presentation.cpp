#include "NativeLabelPresentation.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
    using View=lvui_generated::NativeLabelPresentation;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{180,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto it=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/description");});
    check(it!=nodes.end(),"Label is missing");auto label=take(tree->find(it->id));
    const auto wrapped=take(label.measure(90)).height;scope->backend_0.elide.set(true);
    const auto elided=take(label.measure(90)).height;
    check(wrapped>elided&&elided>0,"Elision did not replace wrapped intrinsic height");
    scope->backend_0.text.set("Updated description");
    check(take(label.snapshot()).text=="Updated description","Reactive label options lost its text");
    auto graph=scope->native_lifetime.lock();scope->backend_0.alignment.set(3);
    check(!take(graph->failures()).empty()&&take(label.snapshot()).text=="Updated description","Invalid alignment replaced the label");
    scope->backend_0.alignment.set(2);check(take(graph->failures()).empty(),"Label did not recover after invalid alignment");
    scope->backend_0.elide.set(false);check(take(label.measure(90)).height>0,"Label wrapping failed to recover");
    good(host.clear_content());scope.reset();graph.reset();check(!label.snapshot(),"Retired label remained active");tree->close();
}
