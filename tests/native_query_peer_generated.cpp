#include "NativeQueryPeer.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string_view>

using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Peer fixture widget missing");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeQueryPeer;
    ii::runtime::EventLoop loop(8);
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,200}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},{},&scope);}));
    std::int64_t remote_received=0;
    ii::runtime::QueryPeer<std::int64_t,std::int64_t> other([&](const std::int64_t& value){remote_received=value;return value*2;});
    check(bool(other.connect(scope->backend_0.peer)),"ViewModel could not link a peer");
    good(find(tree,"/send").activate());
    check(remote_received==7&&take(find(tree,"/reply").snapshot()).text=="14","View did not send a query to its peer");
    auto result=other.send(9);
    check(result&&*result==10&&take(find(tree,"/received").snapshot()).text=="9","Peer-initiated query did not update the view");
    bool completed=false;
    check(bool(other.post(loop,10,[&](auto){completed=true;})),"Could not enqueue a query to the view");
    good(host.clear_content());scope.reset();loop.poll({8,1s});
    check(!completed&&!other.connected(),"Retired view accepted a queued peer query");
    tree->close();
}
