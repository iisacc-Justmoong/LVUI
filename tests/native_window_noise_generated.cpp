#include "NativeWindowNoise.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::shared_ptr<const ii::gui::RasterImage> image(const ii::gui::CanvasFrame& frame){
    if(!frame.images.empty())return frame.images.front().image;
    for(const auto& layer:frame.overlays)if(!layer.images.empty())return layer.images.front().image;
    throw std::runtime_error("Generated window noise has no image");
}
}
int main(){
    using View=lvui_generated::NativeWindowNoise;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto original=image(take(tree->drawing_snapshot({400,300})));scope->backend_0.strength.set(.4);
    auto updated=image(take(tree->drawing_snapshot({400,300})));require(updated!=original,"Changed strength retained old noise");
    auto graph=scope->native_lifetime.lock();scope->backend_0.strength.set(2);
    require(!take(graph->failures()).empty()&&image(take(tree->drawing_snapshot({400,300})))==updated,
            "Invalid reactive strength replaced the retained noise");
    scope->backend_0.strength.set(.2);require(take(graph->failures()).empty(),"Noise did not recover from invalid input");
    std::weak_ptr<View::Scope> lifetime=scope;good(host.clear_content());scope.reset();graph.reset();
    require(lifetime.expired(),"Retired window noise retained its scope");tree->close();
}
