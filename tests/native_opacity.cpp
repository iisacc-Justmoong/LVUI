#ifndef LVUI_OPACITY_HEADER
#define LVUI_OPACITY_HEADER "NativeOpacityRuntime.generated.h"
#define LVUI_OPACITY_VIEW lvui_generated::NativeOpacityRuntime
#endif
#include LVUI_OPACITY_HEADER
#include <iiGUI/WidgetState.h>
#ifdef LVUI_OPACITY_LVRS
#include <LVRS/HierarchyItem.h>
#endif
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(double actual,double expected){check(std::abs(actual-expected)<1e-6,"Generated opacity product differs");}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
using View=LVUI_OPACITY_VIEW;
struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"host",{640,480}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& b){return View::build(b,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    gui::Widget widget(std::string_view suffix){for(const auto& value:take(state->snapshots()))if(value.id.ends_with("/"+std::string(suffix)))return take(state->find(value.id));throw std::runtime_error("Opacity widget not found");}
    bool has(std::string_view suffix){for(const auto& value:take(state->snapshots()))if(value.id.ends_with("/"+std::string(suffix)))return true;return false;}
    void opacity(std::string_view name,double expected){near(take(widget(name).snapshot()).effective_opacity,expected);}
};
void initial(){Mounted m;m.opacity("label",.4);m.opacity("action",.2);m.opacity("pinned",.4);m.opacity("conditional",.4);check(m.scope->backend_0.reads==1,"Shared opacity input ran its C++ method once per consumer");}
void updates(){Mounted m;auto& vm=m.scope->backend_0;vm.alpha.set(.25);m.opacity("label",.2);m.opacity("action",.1);m.opacity("pinned",.4);m.opacity("conditional",.2);check(vm.reads==2,"Opacity input recomputed for each consuming control");vm.container_alpha.set(.4);m.opacity("label",.1);m.opacity("pinned",.2);check(vm.reads==2,"An ancestor opacity change recomputed an unrelated C++ method");}
void invalid(){Mounted m;auto& vm=m.scope->backend_0;vm.alpha.set(1.1);m.opacity("label",.4);m.opacity("conditional",.4);vm.container_alpha.set(1.1);check(!take(m.scope->native_lifetime.lock()->failures()).empty(),"Out-of-range root opacity was not diagnosed");vm.alpha.set(.25);m.opacity("label",.2);m.opacity("conditional",.2);vm.container_alpha.set(.8);check(take(m.scope->native_lifetime.lock()->failures()).empty(),"Valid opacity could not recover after a rejected update");}
void zero(){Mounted m;auto field=m.widget("edit");good(field.focus());m.scope->backend_0.alpha.set(0);m.opacity("edit",0);check(take(field.snapshot()).focused&&take(field.snapshot()).enabled,"Transparent native input lost focus or enabled state");good(m.state->input({.kind=gui::WidgetInputKind::text,.text="!"}));check(take(field.snapshot()).text!="Working","Transparent native editor stopped receiving input");good(m.widget("action").activate());check(m.scope->backend_0.commands==1,"Transparency suppressed a C++ command");}
void identity(){Mounted m;auto field=m.widget("edit");good(field.set_text("Draft"));good(field.focus());const auto before=take(field.snapshot());const auto generation=take(m.host.snapshot()).content_generation;const auto nodes=take(m.state->snapshots()).size();m.scope->backend_0.alpha.set(.1);m.scope->backend_0.container_alpha.set(.5);const auto after=take(field.snapshot());check(after.id==before.id&&after.bounds==before.bounds&&after.focused&&after.text=="Draft"&&nodes==take(m.state->snapshots()).size()&&take(m.host.snapshot()).content_generation==generation,"Opacity rebuilt a scope, reset user input or changed geometry");}
void conditional(){Mounted m;auto old=m.widget("conditional");m.scope->backend_0.shown.set(false);check(!m.has("conditional")&&!old.snapshot(),"Conditional opacity target did not retire");m.scope->backend_0.alpha.set(.25);m.scope->backend_0.shown.set(true);m.opacity("conditional",.2);check(!old.set_opacity(0),"Retired opacity binding targeted the replacement");}
void retirement(){Mounted m;auto old=m.widget("label");std::weak_ptr<typename View::Scope> weak=m.scope;good(m.host.clear_content());m.scope.reset();check(weak.expired()&&NativeOpacityModel::live==0&&!old.set_opacity(.5),"Opacity retained a backend or admitted retired work");}
void nonmutation(){Mounted m;const auto revision=m.scope->backend_0.file_revision;const auto reads=m.scope->backend_0.reads;for(int i=0;i<10;++i){(void)take(m.state->snapshots());(void)take(m.state->drawing_snapshot({640,480}));}m.scope->backend_0.container_alpha.set(.2);m.scope->backend_0.alpha.set(.25);check(m.scope->backend_0.file_revision==revision&&m.scope->backend_0.commands==0&&m.scope->backend_0.reads==reads+1,"View inspection or opacity changed file state or ran an unrelated command");}
#ifdef LVUI_OPACITY_LVRS
void hierarchy(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));lvrs::HierarchyItem item;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        item=take(lvrs::HierarchyItem::create(builder,{},"layer",{.row={.key="layer",.label="Layer",.count=1}}, {},
            [](auto& child,auto parent,const auto&)->core::Result<void>{auto label=take(child.label(parent,"count",{.text="1"}));return label.set_opacity(.5);}));
        return item.root().set_opacity(.4);
    }));
    auto count=take(item.count_content("count"));const auto id=take(count.snapshot()).id;near(take(count.snapshot()).effective_opacity,.2);
    good(item.set_selected(true));auto options=take(item.options());options.row.label="Renamed";options.row.count=2;good(item.set_options(options));
    check(take(item.options()).row.selected&&take(count.snapshot()).id==id,"A native row state update replaced its count slot");
    near(take(item.root().snapshot()).opacity,.4);near(take(count.snapshot()).effective_opacity,.2);state->close();
}
#endif
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"initial";if(name=="initial")initial();else if(name=="updates")updates();else if(name=="invalid")invalid();else if(name=="zero")zero();else if(name=="identity")identity();else if(name=="conditional")conditional();else if(name=="retirement")retirement();else if(name=="nonmutation")nonmutation();
#ifdef LVUI_OPACITY_LVRS
else if(name=="hierarchy")hierarchy();
#endif
else throw std::runtime_error("Unknown native opacity case");check(NativeOpacityModel::live==0,"Opacity scope leaked its C++ backend");std::cout<<"Native opacity "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
