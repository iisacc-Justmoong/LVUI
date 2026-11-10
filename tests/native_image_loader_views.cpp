#include "NativeImageLoaderViews.generated.h"
#include "NativeImageLoaderStaticViews.generated.h"
#include "NativeImageScopedViews.generated.h"
#include "NativeImageScopedStatic.generated.h"
#include "NativeImageScopedNested.generated.h"
#include "NativeImageScopedHidden.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
constexpr std::string_view svg=R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="red"/></svg>)";
constexpr std::string_view green=R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="#00ff00"/></svg>)";
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
struct Request final:lvui::native::ImageRequest {
    lvui::native::ImageCompletion completion;
    bool cancelled=false;
    explicit Request(lvui::native::ImageCompletion value):completion(std::move(value)){}
    void cancel()noexcept override{cancelled=true;}
};
struct Source final:lvui::native::ImageResourceSource {
    std::vector<std::shared_ptr<Request>> requests;
    std::string local=std::string(svg);
    unsigned reads=0;
    ii::core::Result<lvui::native::ImageResource> read(std::string_view address)override{
        ++reads;
        if(address=="fixture.svg")return lvui::native::ImageResource{std::string(address),local};
        return lvui::native::ImageResource{std::string(address),std::nullopt};
    }
    ii::core::Result<std::shared_ptr<lvui::native::ImageRequest>> fetch(std::string_view,lvui::native::ImageCompletion completion)override{
        auto request=std::make_shared<Request>(std::move(completion));requests.push_back(request);return std::shared_ptr<lvui::native::ImageRequest>(request);
    }
};
struct ImageActivationProbe final {
    bool attached=false,retired=false;
    void attach(std::weak_ptr<void>,std::function<void()>){attached=true;}
    void retire(std::shared_ptr<void>){retired=true;}
};
std::size_t red_images(const ii::gui::CanvasFrame& frame){
    std::size_t count=0;
    const auto inspect=[&](const auto& items){for(const auto& item:items){if(item.image){auto rgba=item.image->rgba();if(rgba.size()>=4&&rgba[0]>240&&rgba[1]<10&&rgba[2]<10&&rgba[3]>240)++count;}}};
    inspect(frame.images);for(const auto& overlay:frame.overlays)inspect(overlay.images);return count;
}
std::size_t green_images(const ii::gui::CanvasFrame& frame){
    std::size_t count=0;
    const auto inspect=[&](const auto& items){for(const auto& item:items){if(item.image){auto rgba=item.image->rgba();if(rgba.size()>=4&&rgba[0]<10&&rgba[1]>240&&rgba[2]<10&&rgba[3]>240)++count;}}};
    inspect(frame.images);for(const auto& overlay:frame.overlays)inspect(overlay.images);return count;
}
bool contains_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    const auto found=[&](const auto& items){return std::ranges::any_of(items,[&](const auto& item){return item.image==image;});};
    if(found(frame.images))return true;for(const auto& overlay:frame.overlays)if(found(overlay.images))return true;return false;
}
void static_and_hidden(){
    using View=lvui_generated::NativeImageLoaderStaticViews;
    auto source=std::make_shared<Source>();
    auto loader=std::make_shared<lvui::native::ImageLoader>(source);
    auto pixels=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{0,0,255,255}));
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"static",{800,800}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){value.backend_0.loader.set(loader);value.backend_0.pixels=pixels;},&scope);}));
    const auto frame=take(state->drawing_snapshot({800,800}));
    require(red_images(frame)==1&&contains_image(frame,pixels),"Literal URL or supplied image handle did not render");
    require(source->requests.empty(),"Hidden or unused preview initiated a request");
    scope->backend_0.show.set(true);
    require(source->requests.size()==1,"A newly visible preview did not initiate its request");
    source->requests.front()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==2,"The newly visible preview did not render");
    good(host.clear_content());scope.reset();state->close();
}
void asynchronous_binding(){
    using namespace std::chrono_literals;
    using View=lvui_generated::NativeImageLoaderViews;
    ii::runtime::EventLoop loop(1);
    ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto source=std::make_shared<Source>();
    auto loader=std::make_shared<lvui::native::ImageLoader>(source,loop,runtime);
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"async",{800,800}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){value.backend_0.loader.set(loader);},&scope);}));
    require(source->requests.size()==1,"Asynchronous views did not share the source request");
    source->requests.front()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==0,"Asynchronous decoding completed inline");
    const auto deadline=std::chrono::steady_clock::now()+10s;
    while(red_images(take(state->drawing_snapshot({800,800})))!=4&&std::chrono::steady_clock::now()<deadline){loop.poll();std::this_thread::sleep_for(1ms);}
    require(red_images(take(state->drawing_snapshot({800,800})))==4,"Background raster completions did not update all four views");
    good(host.clear_content());scope.reset();
    auto native_loader=std::make_shared<lvui::native::ImageLoader>(loop,runtime);
    const auto data=std::string("data:image/svg+xml,")+std::string(svg);
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){value.backend_0.loader.set(native_loader);value.backend_0.url.set(data);},&scope);}));
    const auto local_deadline=std::chrono::steady_clock::now()+10s;
    while(red_images(take(state->drawing_snapshot({800,800})))!=4&&std::chrono::steady_clock::now()<local_deadline){loop.poll();std::this_thread::sleep_for(1ms);}
    require(red_images(take(state->drawing_snapshot({800,800})))==4,"Deferred source and raster completion failed with one loop slot");
    good(host.clear_content());scope.reset();state->close();
}
void scoped_bindings(){
    using View=lvui_generated::NativeImageScopedViews;
    auto source=std::make_shared<Source>(),overrides=std::make_shared<Source>();
    auto loader=std::make_shared<lvui::native::ImageLoader>(source);
    auto explicit_loader=std::make_shared<lvui::native::ImageLoader>(overrides);
    auto pixels=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{0,0,255,255}));
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"scoped",{800,800}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){
        value.image_services->resolver.set(loader);value.backend_0.loader.set(explicit_loader);value.backend_0.pixels=pixels;
    },&scope);}));
    require(source->requests.size()==1&&overrides->requests.size()==1,"Scoped and explicit loaders did not each deduplicate requests");
    source->requests.front()->completion(std::string(svg));
    auto frame=take(state->drawing_snapshot({800,800}));
    require(red_images(frame)==4&&contains_image(frame,pixels),"Scoped images or direct pixels did not render");
    overrides->requests.front()->completion(std::string(green));
    require(green_images(take(state->drawing_snapshot({800,800})))==1,"Explicit image loader did not override the scoped service");
    scope->backend_0.show.set(true);
    require(red_images(take(state->drawing_snapshot({800,800})))==5,"Conditional image did not inherit its scoped service");
    auto replacement_source=std::make_shared<Source>();replacement_source->local=std::string(green);
    auto replacement=std::make_shared<lvui::native::ImageLoader>(replacement_source);
    scope->image_services->resolver.set(replacement);
    frame=take(state->drawing_snapshot({800,800}));
    require(replacement_source->requests.size()==1&&red_images(frame)==0&&green_images(frame)==2&&contains_image(frame,pixels),
            "Replacing scoped services affected an override or retained stale images");
    loader->clear();
    require(green_images(take(state->drawing_snapshot({800,800})))==2,"The retired service still changed the view");
    replacement_source->requests.front()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==4,"Replacement scoped service did not update all images");
    scope->backend_0.url.set("https://example.test/next");
    require(replacement_source->requests.size()==2&&overrides->requests.size()==2,"Reactive URL did not reach both services");
    auto final_source=std::make_shared<Source>();auto final_loader=std::make_shared<lvui::native::ImageLoader>(final_source);
    scope->image_services->resolver.set(final_loader);
    require(final_source->requests.size()==1,"The next scoped service did not start its own request");
    replacement_source->requests.back()->completion(std::string(green));
    require(green_images(take(state->drawing_snapshot({800,800})))==0,"A stale service response appeared after replacement");
    overrides->requests.back()->completion(std::string(green));
    final_source->requests.front()->completion(std::string(svg));
    frame=take(state->drawing_snapshot({800,800}));
    require(red_images(frame)==5&&green_images(frame)==1&&contains_image(frame,pixels),"Final scoped and explicit images diverged");
    good(host.clear_content());scope.reset();state->close();
}
void scoped_static_and_nested(){
    using Static=lvui_generated::NativeImageScopedStatic;
    auto red_source=std::make_shared<Source>(),green_source=std::make_shared<Source>();green_source->local=std::string(green);
    auto red_loader=std::make_shared<lvui::native::ImageLoader>(red_source),green_loader=std::make_shared<lvui::native::ImageLoader>(green_source);
    auto a=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto b=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host_a=take(a->content_host({},"a",{300,300})),host_b=take(b->content_host({},"b",{300,300}));
    std::shared_ptr<Static::Scope> scope_a,scope_b;
    good(host_a.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},&scope_a);}));
    require(red_images(take(a->drawing_snapshot({300,300})))==0,"A missing resolver did not leave an empty image");
    scope_a->image_services->resolver.set(red_loader);
    good(host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{},[&](auto& value){value.image_services->resolver.set(green_loader);},&scope_b);}));
    require(scope_a->image_services!=scope_b->image_services&&red_images(take(a->drawing_snapshot({300,300})))==1&&green_images(take(b->drawing_snapshot({300,300})))==1,
            "Separate activations did not isolate their default services");
    scope_a->image_services->resolver.set({});
    require(red_images(take(a->drawing_snapshot({300,300})))==0&&green_images(take(b->drawing_snapshot({300,300})))==1,
            "Removing a scoped resolver retained a stale image or changed another activation");
    scope_a->image_services->resolver.set(green_loader);
    require(green_images(take(a->drawing_snapshot({300,300})))==1&&green_images(take(b->drawing_snapshot({300,300})))==1,
            "Changing one activation modified the other activation");
    std::weak_ptr<lvui::native::ImageServices<ii::gui::RasterImage>> retired=scope_a->image_services;
    good(host_a.clear_content());good(host_b.clear_content());scope_a.reset();scope_b.reset();a->close();b->close();
    require(retired.expired(),"A retired activation retained its image services");

    auto shared=std::make_shared<lvui::native::ImageServices<ii::gui::RasterImage>>();shared->resolver.set(red_loader);
    a=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    b=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    host_a=take(a->content_host({},"shared-a",{300,300}));host_b=take(b->content_host({},"shared-b",{300,300}));
    good(host_a.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{},[&](auto& value){value.image_services=shared;},&scope_a);}));
    good(host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{},[&](auto& value){value.image_services=shared;},&scope_b);}));
    shared->resolver.set(green_loader);
    require(green_images(take(a->drawing_snapshot({300,300})))==1&&green_images(take(b->drawing_snapshot({300,300})))==1,
            "Explicitly shared services did not update both activations");
    good(host_a.clear_content());good(host_b.clear_content());scope_a.reset();scope_b.reset();a->close();b->close();

    using Nested=lvui_generated::NativeImageScopedNested;
    auto nested_source=std::make_shared<Source>();auto nested_loader=std::make_shared<lvui::native::ImageLoader>(nested_source);
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"nested-host",{300,300}));std::shared_ptr<Nested::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return Nested::build(widgets,{},[&](auto& value){value.image_services->resolver.set(nested_loader);},&scope);}));
    require(nested_source->reads==0,"A hidden lexical scope loaded its image");
    scope->backend_0.show.set(true);
    require(nested_source->reads==1&&red_images(take(state->drawing_snapshot({300,300})))==1,"A lexical child did not inherit image services");
    scope->image_services->resolver.set(green_loader);
    require(green_images(take(state->drawing_snapshot({300,300})))==1,"A lexical child did not observe service replacement");
    good(host.clear_content());scope.reset();state->close();
}
void scoped_hidden_without_service(){
    using View=lvui_generated::NativeImageScopedHidden;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"hidden",{400,400}));
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets);}));
    require(red_images(take(state->drawing_snapshot({400,400})))==0,"Empty or hidden sources unexpectedly drew an image");
    good(host.clear_content());state->close();
}
void scoped_factory_services(){
    using Static=lvui_generated::NativeImageScopedStatic;
    using Services=lvui::native::ImageServices<ii::gui::RasterImage>;
    auto red_source=std::make_shared<Source>(),green_source=std::make_shared<Source>();green_source->local=std::string(green);
    auto red_loader=std::make_shared<lvui::native::ImageLoader>(red_source),green_loader=std::make_shared<lvui::native::ImageLoader>(green_source);
    auto shared=std::make_shared<Services>();shared->resolver.set(red_loader);
    int calls=0;
    const auto factories=lvui::native::BackendFactories{}.with<std::shared_ptr<Services>>([&](const lvui::native::BackendRequest& request){
        require(request.type=="lvui::native::ImageServices"&&request.binding=="image_services","Image service factory lost its construction site");
        ++calls;return shared;
    });
    auto a=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto b=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host_a=take(a->content_host({},"factory-a",{300,300})),host_b=take(b->content_host({},"factory-b",{300,300}));
    std::shared_ptr<Static::Scope> scope_a,scope_b;
    good(host_a.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},&scope_a,factories);}));
    good(host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},&scope_b,factories);}));
    require(calls==2&&scope_a->image_services==shared&&scope_b->image_services==shared&&
            red_images(take(a->drawing_snapshot({300,300})))==1&&red_images(take(b->drawing_snapshot({300,300})))==1,
            "The application image policy was not installed once per activation");
    shared->resolver.set(green_loader);
    require(green_images(take(a->drawing_snapshot({300,300})))==1&&green_images(take(b->drawing_snapshot({300,300})))==1,
            "A shared application image policy did not update both views");
    auto local=std::make_shared<Services>();local->resolver.set(red_loader);
    good(host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{},[&](auto& value){value.image_services=local;},&scope_b,factories);}));
    require(calls==3&&scope_b->image_services==local&&red_images(take(b->drawing_snapshot({300,300})))==1,
            "An activation could not override the application image policy");
    const auto throwing=factories.with<std::shared_ptr<Services>>([]()->std::shared_ptr<Services>{throw std::runtime_error("image service failure");});
    auto failed=host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},nullptr,throwing);});
    require(!failed&&red_images(take(b->drawing_snapshot({300,300})))==1,
            "A failed image service factory changed the previous view");
    bool original_error=false;
    try{if(!failed&&failed.error().cause)std::rethrow_exception(failed.error().cause);}
    catch(const std::runtime_error& error){original_error=std::string(error.what())=="image service failure";}
    require(original_error,"An image service factory lost its original exception");
    const auto empty=factories.with<std::shared_ptr<Services>>([]{return std::shared_ptr<Services>{};});
    good(host_b.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},&scope_b,empty);}));
    require(scope_b->image_services&&red_images(take(b->drawing_snapshot({300,300})))==0,
            "An empty service factory did not restore an independent empty slot");
    auto probe=std::make_shared<ImageActivationProbe>();int activations=0;
    const auto activated=factories.with_activation([&]{++activations;return lvui::native::BackendActivation::from(probe);})
        .with<std::shared_ptr<Services>>([&](const lvui::native::BackendRequest& request){
            require(request.activation.get<ImageActivationProbe>()==probe,"Image-only view lost its activation context");return shared;
        });
    good(host_a.set_content([&](ii::gui::WidgetBuilder& widgets){return Static::build(widgets,{}, {},&scope_a,activated);}));
    require(activations==1&&probe->attached,"Image-only activation was not attached exactly once");
    good(host_a.clear_content());
    require(probe->retired,"Image-only activation was not retired with its view");
    good(host_a.clear_content());good(host_b.clear_content());scope_a.reset();scope_b.reset();a->close();b->close();

    using Nested=lvui_generated::NativeImageScopedNested;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"factory-nested",{300,300}));std::shared_ptr<Nested::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return Nested::build(widgets,{},[&](auto& value){value.backend_0.show.set(true);},&scope,factories);}));
    require(calls==4&&scope->image_services==shared&&green_images(take(state->drawing_snapshot({300,300})))==1,
            "A nested scope did not inherit the application image policy");
    good(host.clear_content());scope.reset();state->close();
}
void scoped_async(){
    using namespace std::chrono_literals;
    using View=lvui_generated::NativeImageScopedViews;
    ii::runtime::EventLoop loop(1);ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto source=std::make_shared<Source>();auto loader=std::make_shared<lvui::native::ImageLoader>(source,loop,runtime);
    auto pixels=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{0,0,255,255}));
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"scoped-async",{800,800}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){
        value.image_services->resolver.set(loader);value.backend_0.loader.set(loader);value.backend_0.pixels=pixels;
    },&scope);}));
    require(source->requests.size()==1,"Scoped asynchronous requests were not shared");
    source->requests.front()->completion(std::string(svg));
    auto deadline=std::chrono::steady_clock::now()+10s;
    while(red_images(take(state->drawing_snapshot({800,800})))!=5&&std::chrono::steady_clock::now()<deadline){loop.poll();std::this_thread::sleep_for(1ms);}
    require(red_images(take(state->drawing_snapshot({800,800})))==5,"Scoped asynchronous completion did not update the views");
    scope->backend_0.show.set(true);deadline=std::chrono::steady_clock::now()+10s;
    while(red_images(take(state->drawing_snapshot({800,800})))!=6&&std::chrono::steady_clock::now()<deadline){loop.poll();std::this_thread::sleep_for(1ms);}
    require(red_images(take(state->drawing_snapshot({800,800})))==6,"A late scoped child missed asynchronous completion");
    good(host.clear_content());scope.reset();state->close();
}
void scoped_late_service(){
    using View=lvui_generated::NativeImageScopedViews;
    auto source=std::make_shared<Source>();auto loader=std::make_shared<lvui::native::ImageLoader>(source);
    auto pixels=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{0,0,255,255}));
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"late",{800,800}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){
        value.backend_0.url.set("");value.backend_0.loader.set(loader);value.backend_0.pixels=pixels;
    },&scope);}));
    require(source->requests.empty()&&contains_image(take(state->drawing_snapshot({800,800})),pixels),"Empty scoped URLs should not require a resolver");
    scope->backend_0.url.set("https://example.test/late");
    require(source->requests.size()==1,"The explicit loader did not start");
    scope->image_services->resolver.set(loader);
    source->requests.front()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==5,"Late scoped service did not recover its image bindings");
    const auto failures=take(scope->binding_failures());
    require(failures.empty(),"Recovered scoped images retained binding errors");
    good(host.clear_content());scope.reset();state->close();
}
}
int main(){
    using View=lvui_generated::NativeImageLoaderViews;
    auto source=std::make_shared<Source>();
    auto loader=std::make_shared<lvui::native::ImageLoader>(source);
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{800,800}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{},[&](auto& value){value.backend_0.loader.set(loader);},&scope);}));
    require(source->requests.size()==1&&red_images(take(state->drawing_snapshot({800,800})))==0,"Views did not share the pending URL");
    auto first=source->requests.front();
    scope->backend_0.url.set("https://example.test/second");
    require(source->requests.size()==2,"Reactive URL change did not request the replacement");
    first->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==0,"An old URL appeared in the current views");
    source->requests.back()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==4,"Image, avatar, card and list preview did not receive the decoded image");
    auto replacement_source=std::make_shared<Source>();
    auto replacement=std::make_shared<lvui::native::ImageLoader>(replacement_source);
    scope->backend_0.loader.set(replacement);
    require(replacement_source->requests.size()==1&&red_images(take(state->drawing_snapshot({800,800})))==0,"Replacing the resolver did not retire the previous image");
    loader->clear();
    require(red_images(take(state->drawing_snapshot({800,800})))==0,"The previous resolver changed the replacement views");
    replacement_source->requests.front()->completion(std::string(svg));
    require(red_images(take(state->drawing_snapshot({800,800})))==4,"Replacement resolver completion did not update the views");
    scope->backend_0.url.set("");
    require(red_images(take(state->drawing_snapshot({800,800})))==0,"Clearing the URL did not clear the views");
    std::weak_ptr<View::Scope> weak=scope;
    good(host.clear_content());scope.reset();
    require(weak.expired(),"Retired image views retained their scope");
    state->close();
    static_and_hidden();
    asynchronous_binding();
    scoped_bindings();
    scoped_static_and_nested();
    scoped_late_service();
    scoped_hidden_without_service();
    scoped_factory_services();
    scoped_async();
}
