#include "NativeOwnedCardSlots.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using View=lvui_generated::NativeOwnedCardSlots;
using State=ii::gui::detail::WidgetState;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
ii::gui::Widget find(const std::shared_ptr<State>& tree,std::string_view suffix){
    for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/"+std::string(suffix)))return take(tree->find(item.id));
    throw std::runtime_error("Card slot widget not found: "+std::string(suffix));
}
}
int main(){
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{600,800}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto value=find(tree,"card-value"),button=find(tree,"card-button"),caption=find(tree,"card-caption");
    auto preview=find(tree,"custom-preview"),icon=find(tree,"custom-icon");
    require(take(value.snapshot()).text=="0"&&take(preview.snapshot()).text=="Initial"&&take(icon.snapshot()).text=="Initial","Card View slots did not mount");
    good(button.activate());require(take(value.snapshot()).text=="1"&&scope->backend_0.clicks==0,"Card content lost its independent action or state");
    scope->backend_0.caption.set("Updated");
    require(take(caption.snapshot()).text=="Updated"&&take(preview.snapshot()).text=="Updated"&&take(icon.snapshot()).text=="Updated","Card View slots lost reactive ViewModel values");
    good(host.clear_content());require(!button.snapshot()&&!preview.snapshot()&&!icon.snapshot(),"Card slots survived retirement");tree->close();
}
