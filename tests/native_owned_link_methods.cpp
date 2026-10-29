#include "NativeOwnedLinkMethods.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using View=lvui_generated::NativeOwnedLinkMethods;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{300,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    const auto snapshots=take(state->snapshots());const auto item=std::ranges::find_if(snapshots,[](const auto& value){return value.id.ends_with("/link");});
    require(item!=snapshots.end(),"Dynamic methods Link missing");auto link=take(state->find(item->id));auto graph=scope->native_lifetime.lock();
    std::vector<std::string> events;scope->backend_0.methods={[&](const auto&){events.push_back("first");}};scope->publish_native_changes();
    good(link.activate());require(events==std::vector<std::string>{"first"},"Live Link method did not update");
    scope->backend_0.methods=std::vector<lvui::LinkMethod>(257,[](const auto&){});bool rejected=false;
    try{scope->publish_native_changes();}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&!take(graph->failures()).empty(),"Excess methods were not reported");good(link.activate());
    require(events==std::vector<std::string>({"first","first"}),"Rejected methods changed the live Link");
    scope->backend_0.methods={[&](const auto&){events.push_back("retire");good(host.clear_content());},
                              [&](const auto&){events.push_back("unexpected");}};scope->publish_native_changes();
    require(take(graph->failures()).empty(),"Link methods did not recover");good(link.activate());
    require(events==std::vector<std::string>({"first","first","retire"})&&!link.snapshot(),"Retirement did not stop remaining methods");
    std::weak_ptr<View::Scope> weak=scope;scope.reset();graph.reset();require(weak.expired(),"Retired methods retained Link scope");state->close();
}
