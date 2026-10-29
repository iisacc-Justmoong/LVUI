#include <LVUI/NativeWork.h>
#include <condition_variable>
#include <iostream>
#include <stdexcept>
using namespace lvui::native;
using namespace ii::runtime;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>concept WorkAccepted=requires(WorkScope& scope,F function){scope.post(function);};
template<class F>concept DeliveryAccepted=requires(WorkScope& scope,F function){scope.deliver(function);};
static_assert(!WorkAccepted<decltype([](std::stop_token){return 1;})>);
static_assert(!DeliveryAccepted<decltype([]{return 1;})>);
struct Gate final {std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
    void enter(){std::unique_lock lock(mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,5s,[&]{return released;}))throw std::runtime_error("Gate timeout");}
    void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,5s,[&]{return entered;}))throw std::runtime_error("Work timeout");}
    void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
};
struct Owner final {int value=0;std::atomic_int used=0;std::thread::id thread=std::this_thread::get_id();
    static inline bool wrong_destruction=false;std::function<void()> on_destroy;
    ~Owner(){if(thread!=std::this_thread::get_id())wrong_destruction=true;if(on_destroy)on_destroy();}
};
struct Fixture final {
    Runtime workers{{.workers=1,.capacity=8}};EventLoop ui{16};WorkRuntime runtime{workers,ui.sender()};
    BackendActivation activation=runtime.factories().activate();
    std::shared_ptr<WorkScope> context=activation.get<WorkGroup>()->scope("test");std::shared_ptr<Owner> owner=std::make_shared<Owner>();int changes=0;
    Fixture(){Owner::wrong_destruction=false;activation.attach(owner,[&]{++changes;});}
    ~Fixture(){(void)runtime.join();}
};
void queued(){
    Fixture f;Gate unrelated;check(bool(f.workers.post([&]{unrelated.enter();})),"Blocker rejected");unrelated.wait();
    auto resource=std::make_shared<int>(1);std::weak_ptr<int> capture=resource;bool ran=false;
    check(bool(f.context->post([resource,&ran](auto){ran=true;})),"Queued work rejected");resource.reset();
    std::weak_ptr<Owner> retired=f.owner;f.activation.retire(f.owner);f.owner.reset();f.runtime.poll();
    check(retired.expired()&&capture.expired()&&!ran&&f.runtime.stats().retiring==0&&!Owner::wrong_destruction,"Queued removal retained its receiver, captures or resources");
    unrelated.release();check(bool(f.workers.wait_idle()),"Unrelated work did not finish");
}
void active(){
    Fixture f;Gate gate;auto* raw=f.owner.get();
    std::atomic_bool stopped=false;std::atomic<SubmitError> late=SubmitError::none;
    check(bool(f.context->post([&](auto stop){gate.enter();raw->used=1;stopped=stop.stop_requested();late=f.context->deliver([]{}).error;})),"Active work rejected");gate.wait();
    std::weak_ptr<Owner> retired=f.owner;f.activation.retire(f.owner);f.owner.reset();f.runtime.poll();
    check(!retired.expired()&&f.runtime.stats().retiring==1,"Retirement waited or released an active receiver");
    gate.release();check(bool(f.workers.wait_idle()),"Active work did not return");f.runtime.poll();
    check(stopped&&late==SubmitError::stopped&&retired.expired()&&!Owner::wrong_destruction,"Cancellation, late delivery or UI destruction failed");
}
void delivery(){
    Fixture f;auto* raw=f.owner.get();
    check(bool(f.context->post([&f,raw,value=std::make_unique<int>(7)](auto)mutable{check(bool(f.context->deliver([raw,value=std::move(value)]{raw->value=*value;})),"UI result rejected");})),"Work rejected");
    check(bool(f.workers.wait_idle()),"Work failed");f.ui.poll({64,1s});
    check(raw->value==7&&f.changes==1,"A native result did not run and publish on the UI domain");
    std::weak_ptr<Owner> retired=f.owner;
    check(bool(f.context->deliver([&]{f.activation.retire(f.owner);f.owner.reset();f.runtime.poll();check(!retired.expired(),"An active completion lost its receiver");})),"Self-retiring result rejected");
    f.ui.poll({64,1s});f.runtime.poll();
    check(retired.expired()&&!Owner::wrong_destruction,"A completion removing its own view did not retire safely");
}
void errors(){
    Fixture f;check(bool(f.context->post([](auto){throw std::runtime_error("Original worker failure");})),"Failing work rejected");check(bool(f.workers.wait_idle()),"A worker failure escaped its activation");
    auto error=f.context->failure();check(error&&error->message.find("Original worker failure")!=std::string::npos&&bool(error->cause),"Worker diagnostics lost their original evidence");
    check(bool(f.context->post([](auto){}))&&bool(f.workers.wait_idle()),"A failed operation stopped unrelated activation work");
    struct ReceiverError final {std::shared_ptr<Owner> receiver;};
    check(bool(f.context->post([receiver=f.owner](auto){throw ReceiverError{receiver};}))&&bool(f.workers.wait_idle()),"Capturing failure was not isolated");
    std::weak_ptr<Owner> retired=f.owner;f.activation.retire(f.owner);f.owner.reset();f.runtime.poll();
    check(retired.expired()&&f.context->failure()&&!f.context->failure()->cause,"An owned exception retained a retired receiver");
}
void bounds(){
    Runtime workers({.workers=1,.capacity=4});EventLoop ui(1);WorkRuntime runtime(workers,ui.sender(),{.maximum_scopes=1});
    auto factories=runtime.factories();auto activation=factories.activate();auto context=activation.get<WorkGroup>()->scope("first");
    check(context->post([](auto){}).error==SubmitError::stopped,"A constructor started work before owner attachment");
    bool full=false;try{auto extra=factories.activate();(void)extra.get<WorkGroup>()->scope("extra");}catch(const std::length_error&){full=true;}check(full,"Backend work capacity was not enforced");
    auto owner=std::make_shared<Owner>();activation.attach(owner);check(bool(ui.post([]{})),"UI blocker rejected");
    bool wrong=false;try{activation.retire(std::make_shared<Owner>());}catch(const std::logic_error&){wrong=true;}check(wrong&&!context->closing(),"Retirement accepted a different activation owner");
    activation.retire(owner);owner.reset();check(runtime.stats().retirement_dispatch==SubmitError::full,"Retirement dispatch failure was hidden");
    ui.poll({64,1s});runtime.poll();check(runtime.stats().retiring==0,"Explicit polling did not recover from a full UI executor");
}
void routing(){
    Fixture f;auto* raw=f.owner.get();check(bool(f.context->deliver([raw]{raw->value=99;})),"UI result rejected");
    std::thread wrong([&]{f.ui.poll({64,1s});});wrong.join();
    check(raw->value==0&&f.context->failure().has_value(),"A wrongly routed executor touched UI state before rejecting delivery");
}
void parallel(){
    Runtime workers({.workers=2,.capacity=8});EventLoop ui(16);WorkRuntime runtime(workers,ui.sender());
    auto activation=runtime.factories().activate();auto group=activation.get<WorkGroup>();
    auto left=group->scope("left"),right=group->scope("right");auto owner=std::make_shared<Owner>();activation.attach(owner);
    Gate held,independent;
    check(bool(left->post([&](auto){held.enter();})),"Left backend rejected work");held.wait();
    check(bool(right->post([&](auto){{std::lock_guard lock(independent.mutex);independent.entered=true;independent.changed.notify_all();}})),"Right backend rejected work");
    independent.wait();
    std::weak_ptr<Owner> retired=owner;activation.retire(owner);owner.reset();runtime.poll();check(!retired.expired(),"One backend retired its peer's active receiver");
    held.release();check(bool(workers.wait_idle()),"Parallel backend work failed");runtime.poll();check(retired.expired(),"Independent backend queues retained their common view");
}
void reentrant(){
    Runtime workers({.workers=1,.capacity=8});EventLoop ui(16);WorkRuntime runtime(workers,ui.sender());
    auto activation=runtime.factories().activate();auto group=activation.get<WorkGroup>();auto left=group->scope("left"),right=group->scope("right");
    auto owner=std::make_shared<Owner>();activation.attach(owner);std::stop_token stop;
    check(bool(left->post([&](auto token){stop=token;}))&&bool(workers.wait_idle()),"Stop token setup failed");
    Gate blocked;check(bool(workers.post([&]{blocked.enter();})),"Blocker rejected");blocked.wait();
    auto queued=right->post([](auto){});check(bool(queued),"Right work rejected");
    std::weak_ptr<Owner> retired=owner;bool safe=false;
    owner->on_destroy=[&]{safe=left->closing()&&right->closing();};
    std::stop_callback callback(stop,[&]{(void)queued.token.cancel();runtime.poll();check(!retired.expired(),"A stop callback destroyed its active receiver");});
    owner.reset();(void)runtime.close();runtime.poll();
    check(retired.expired()&&safe,"Reentrant shutdown destroyed a receiver before closing all backend admission");
    blocked.release();check(bool(workers.wait_idle()),"Blocker failed");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"queued";
    if(name=="queued")queued();else if(name=="active")active();else if(name=="delivery")delivery();else if(name=="errors")errors();else if(name=="bounds")bounds();else if(name=="routing")routing();else if(name=="parallel")parallel();else if(name=="reentrant")reentrant();else throw std::runtime_error("Unknown native work test");
    std::cout<<"Native activation work "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
