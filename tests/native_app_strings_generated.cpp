#include "LocalizedCounter.generated.h"
#include "NativeLocalizedScenes.generated.h"
#include "NativeLocalizedApplication.generated.h"
#include <LVUI/NativeAppStrings.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
using State=ii::gui::detail::WidgetState;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
ii::gui::Widget find(const std::shared_ptr<State>& tree,std::string_view name){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(name)||(name.starts_with('/')&&node.id==name.substr(1));});
    if(found==nodes.end())throw std::runtime_error("Localized fixture widget missing: "+std::string(name));return take(tree->find(found->id));
}
std::string shown(const std::shared_ptr<State>& tree,std::string_view name){return take(find(tree,name).snapshot()).text;}
ii::gui::Widget find_text(const std::shared_ptr<State>& tree,std::string_view text){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.text==text;});
    if(found==nodes.end())throw std::runtime_error("Localized example text missing: "+std::string(text));return take(tree->find(found->id));
}

void views(){
    using View=lvui_generated::LocalizedCounter;
    auto strings=std::make_shared<lvui::native::AppStrings>();
    good(strings->replace_catalog("en",{{"window.title","Counter"},{"counter.heading","Count"},
        {"counter.increment","Add"},{"language.korean","Korean"},{"counter.replace","Replace"},{"counter.replacement","Changed"}}));
    good(strings->replace_catalog("ko",{{"window.title","카운터"},{"counter.heading","숫자"},
        {"counter.increment","더하기"},{"language.korean","한국어"},{"counter.replace","바꾸기"},{"counter.replacement","바뀐 제목"}}));
    auto factories=lvui::native::BackendFactories{}.with_app_strings(strings);
    ii::gui::WindowGeometryValue geometry{"initial",{640,420},{320,220}};
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.window_geometry=[&](const ii::gui::WindowGeometry& update)->ii::core::Result<void>{
        auto value=ii::gui::resolve_window_geometry(geometry,update);if(!value)return std::unexpected(value.error());geometry=*value;return {};
    };
    auto first=std::make_shared<State>([](ii::runtime::Task task){task();},View::appearance(),hooks);
    auto second=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto isolated=std::make_shared<State>([](ii::runtime::Task task){task();});
    std::shared_ptr<View::Scope> a,b,c;
    good(first->construct([&](auto& builder){return View::build(builder,{}, {},&a,factories);}));good(first->commit_window_properties());
    good(second->construct([&](auto& builder){return View::build(builder,{}, {},&b,factories);}));
    good(isolated->construct([&](auto& builder){return View::build(builder,{}, {},&c);}));
    check(shown(first,"/heading")=="Count"&&geometry.title=="Counter"&&shown(isolated,"/heading")=="counter.heading",
          "Initial app strings or isolated fallback failed");
    check(c->backend_factories.app_strings()!=strings,"Independent activation reused the other app store");
    good(find(first,"/increment").activate());
    check(shown(first,"/count")=="1"&&shown(second,"/count")=="0","Button state leaked between sessions");
    good(find(first,"/chooseReplacement").activate());
    check(shown(first,"/dynamic")=="Changed"&&shown(second,"/dynamic")=="Count","Reactive localization key did not update independently");
    good(find(first,"/korean").activate());
    check(shown(first,"/heading")=="숫자"&&shown(second,"/heading")=="숫자"&&geometry.title=="카운터",
          "Locale switch did not update shared label or window title");
    check(shown(first,"/dynamic")=="바뀐 제목"&&shown(second,"/dynamic")=="숫자","Localized key did not follow the active locale");
    check(shown(first,"/initial")=="Count"&&shown(first,"/draft")=="Count"&&shown(isolated,"/heading")=="counter.heading",
          "Locale change overwrote static, mutable, or independent state");
    good(find(first,"/replace").activate());
    check(shown(first,"/heading")=="바뀐 제목"&&shown(second,"/heading")=="바뀐 제목",
          "Runtime string replacement did not update all active views");
    first->close();second->close();isolated->close();a.reset();b.reset();c.reset();
}
void scenes(){
    using View=lvui_generated::NativeLocalizedScenes;
    auto owner=View::make_application_owner();good(owner->activate());
    auto storage=owner->scope()->backend_factories.app_strings();
    check(bool(storage),"Application did not create its own app string storage");
    good(storage->replace_catalog("en",{{"counter.heading","First"}}));
    good(storage->replace_catalog("ko",{{"counter.heading","둘째"}}));
    auto one=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto two=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto h1=take(one->content_host({},"one",{320,200})),h2=take(two->content_host({},"two",{320,200}));
    good(h1.set_content([&](auto& widgets){return View::build_scene_with_application(widgets,{},owner,0);}));
    good(h2.set_content([&](auto& widgets){return View::build_scene_with_application(widgets,{},owner,1);}));
    check(shown(one,"/mainLabel")=="First"&&shown(two,"/toolsLabel")=="First","Scenes did not share application strings");
    good(storage->set_locale("ko"));
    check(shown(one,"/mainLabel")=="둘째"&&shown(two,"/toolsLabel")=="둘째","Locale change did not reach every scene");
    good(h1.clear_content());good(h2.clear_content());good(owner->retire());one->close();two->close();
}
void application_example(){
    using View=lvui_generated::NativeLocalizedApplication;
    auto owner=View::make_application_owner();good(owner->activate());
    auto strings=owner->scope()->backend_factories.app_strings();
    good(strings->replace_catalog("en",{{"window.title","Counter"},{"counter.heading","Count"},
        {"counter.increment","Add"},{"language.korean","Korean"},{"counter.replace","Replace"},{"counter.replacement","Changed"}}));
    good(strings->replace_catalog("ko",{{"window.title","카운터"},{"counter.heading","숫자"},
        {"counter.increment","더하기"},{"language.korean","한국어"},{"counter.replace","바꾸기"},{"counter.replacement","바뀐 제목"}}));
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"example",{320,200}));
    good(host.set_content([&](auto& widgets){return View::build_with_application(widgets,{},owner);}));
    auto heading=find_text(tree,"Count"),count=find_text(tree,"0");
    auto increment=find_text(tree,"Add"),locale=find_text(tree,"Korean"),replace=find_text(tree,"Replace");
    good(increment.activate());
    check(take(count.snapshot()).text=="1","Application example did not update its counter");
    good(locale.activate());
    check(take(heading.snapshot()).text=="숫자","Application example did not update its locale");
    good(replace.activate());
    check(take(heading.snapshot()).text=="바뀐 제목","Application example did not replace the localized value");
    good(host.clear_content());good(owner->retire());tree->close();
}
}
int main(){views();scenes();application_example();}
