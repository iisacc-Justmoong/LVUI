#include <LVUI/TooltipController.h>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Port final:lvui::TooltipPresentationPort {
    std::optional<lvui::TooltipLayout> layout;
    std::optional<std::int64_t> deadline;
    bool fail_present=false,throw_present=false,fail_deadline=false,partial_present=false,uncertain=false;
    int deadline_calls=0,fail_deadline_call=-1;
    std::function<void()> during_present;
    ii::core::Result<void> present(std::optional<lvui::TooltipLayout> next)override{
        if(during_present)during_present();
        if(partial_present){partial_present=false;layout=std::move(next);uncertain=true;return ii::core::fail(ii::core::ErrorCode::failed,"Partial presentation");}
        if(throw_present){throw_present=false;throw std::runtime_error("Presentation failed");}
        if(fail_present){fail_present=false;return ii::core::fail(ii::core::ErrorCode::failed,"Presentation rejected");}
        layout=std::move(next);uncertain=false;return {};
    }
    ii::core::Result<void> set_deadline(std::optional<std::int64_t> next)override{
        ++deadline_calls;if(fail_deadline||deadline_calls==fail_deadline_call){fail_deadline=false;return ii::core::fail(ii::core::ErrorCode::full,"Scheduler rejected");}
        deadline=next;return {};
    }
    bool needs_reconciliation()const noexcept override{return uncertain;}
};
lvui::TooltipGeometryInput geometry(){return {{0,0,300,200},{150,100},{100,30}};}
lvui::TooltipInteraction active(){return {.target_present=true,.target_available=true,.target_engaged=true};}
lvui::TooltipConfiguration options(){lvui::TooltipConfiguration value;value.delay=0ms;value.timeout=100ms;return value;}
void presentation_and_failure(){
    auto port=std::make_shared<Port>();int opened=0,closed=0;
    auto controller=take(lvui::TooltipController::create(port,options(),{.opened=[&]{++opened;},.closed=[&]{++closed;}}));
    good(controller.set_geometry(geometry()));good(controller.advance(0,active()));
    auto before=take(controller.snapshot());require(before.phase==lvui::TooltipPhase::shown&&before.layout&&port->layout==before.layout&&
        port->deadline==100&&opened==1,"Tooltip presentation or deadline was not committed");
    auto moved=geometry();moved.anchor.x=160;port->fail_present=true;
    auto rejected=controller.set_geometry(moved);require(!rejected&&take(controller.snapshot()).layout==before.layout&&port->layout==before.layout,
        "Rejected geometry replaced the visible tooltip");
    port->fail_present=true;rejected=controller.tick(100);
    require(!rejected&&take(controller.snapshot()).phase==lvui::TooltipPhase::shown&&port->layout==before.layout&&port->deadline==100&&closed==0,
        "Failed close did not restore the prior deadline and presentation");
    good(controller.tick(100));require(take(controller.snapshot()).phase==lvui::TooltipPhase::hidden&&!port->layout&&!port->deadline&&closed==1,
        "Tooltip did not close after host recovery");
}
void scheduling_and_exceptions(){
    auto port=std::make_shared<Port>();auto controller=take(lvui::TooltipController::create(port,options()));good(controller.set_geometry(geometry()));
    port->fail_deadline=true;auto rejected=controller.advance(0,active());
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::full&&!port->layout&&!port->deadline&&
        take(controller.snapshot()).phase==lvui::TooltipPhase::hidden,"Scheduler failure committed tooltip state");
    port->throw_present=true;auto thrown=controller.advance(0,active());
    require(!thrown&&thrown.error().cause&&!port->layout&&!port->deadline&&take(controller.snapshot()).phase==lvui::TooltipPhase::hidden,
        "A thrown presentation did not restore scheduling or preserve its cause");
    good(controller.advance(0,active()));require(port->layout&&port->deadline==100,"Tooltip did not recover from host failures");
    bool busy=false;port->during_present=[&]{auto nested=controller.close(1);busy=!nested&&nested.error().code==ii::core::ErrorCode::busy;};
    auto moved=geometry();moved.anchor.x=170;good(controller.set_geometry(moved));port->during_present={};
    require(busy&&take(controller.snapshot()).phase==lvui::TooltipPhase::shown,"A host reentered an active tooltip update");
    bool wrong_thread=false;std::thread other([&]{auto result=controller.tick(2);wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});other.join();
    require(wrong_thread,"A foreign thread advanced the tooltip");
    port->fail_present=true;auto failed_retire=controller.retire();
    require(!failed_retire&&port->layout&&port->deadline==100&&take(controller.snapshot()).phase==lvui::TooltipPhase::shown,
        "Failed retirement did not restore the active deadline");
    good(controller.retire());require(!port->layout&&!port->deadline,"Retirement left the tooltip visible or scheduled");
}
void callback_and_lifetime(){
    auto port=std::make_shared<Port>();std::optional<lvui::TooltipController> controller;int closed=0;
    controller=take(lvui::TooltipController::create(port,options(),{
        .opened=[&]{good(controller->close(1));},.closed=[&]{++closed;}}));
    good(controller->set_geometry(geometry()));good(controller->advance(0,active()));
    require(closed==1&&!port->layout&&!port->deadline&&take(controller->snapshot()).phase==lvui::TooltipPhase::hidden,
        "Opened callback could not safely close the tooltip");
    auto weak=controller->weak_tick();good(controller->retire());auto retired=weak(2);
    require(!retired&&retired.error().code==ii::core::ErrorCode::closed,"A retired tooltip accepted a timer");
    controller.reset();auto expired=weak(3);require(!expired&&expired.error().code==ii::core::ErrorCode::closed,"A timer retained an expired tooltip");

    auto other=std::make_shared<Port>();auto throwing=take(lvui::TooltipController::create(other,options(),{
        .opened=[]{throw std::runtime_error("Observer failed");}}));good(throwing.set_geometry(geometry()));
    auto error=throwing.advance(0,active());require(!error&&error.error().cause&&take(throwing.snapshot()).phase==lvui::TooltipPhase::shown&&other->layout,
        "Callback exception did not preserve the committed presentation and cause");
}
void failed_rollback(){
    auto port=std::make_shared<Port>();auto controller=take(lvui::TooltipController::create(port,options()));good(controller.set_geometry(geometry()));
    port->fail_present=true;port->fail_deadline_call=port->deadline_calls+2;
    auto error=controller.advance(0,active());require(!error&&error.error().code==ii::core::ErrorCode::failed&&
        take(controller.snapshot()).phase==lvui::TooltipPhase::hidden&&take(controller.snapshot()).needs_reconciliation&&port->deadline==100,
        "A failed scheduler rollback was hidden");
    require(!controller.tick(1),"An unreconciled tooltip accepted an update");good(controller.reconcile());
    require(!take(controller.snapshot()).needs_reconciliation&&!port->deadline&&!port->layout,"Reconciliation did not restore host state");
    good(controller.advance(1,active()));require(port->layout&&port->deadline==101,"Tooltip did not recover after reconciliation");
}
void timer_delivery(){
    auto empty_port=std::make_shared<Port>();auto empty=take(lvui::TooltipController::create(empty_port,options()));
    auto missing=empty.advance(0,active());require(!missing&&missing.error().code==ii::core::ErrorCode::invalid_argument&&
        !empty_port->layout&&!empty_port->deadline,"Missing geometry scheduled a visible tooltip");
    auto port=std::make_shared<Port>();auto timing=options();timing.delay=50ms;
    auto controller=take(lvui::TooltipController::create(port,timing));auto wake=controller.weak_tick();
    good(controller.set_geometry(geometry()));good(controller.advance(0,active()));
    require(port->deadline==50&&!port->layout,"Delayed tooltip was presented too soon");
    good(wake(25));require(port->deadline==50&&!port->layout,"An early wake changed a pending tooltip");
    good(wake(50));require(port->deadline==150&&port->layout,"A due wake did not present the tooltip");
    good(wake(51));require(port->deadline==150&&port->layout,"A stale wake changed the current timeout");
    good(wake(150));require(!port->deadline&&!port->layout,"A due timeout did not dismiss the tooltip");
}
void partial_presentation(){
    auto port=std::make_shared<Port>();auto controller=take(lvui::TooltipController::create(port,options()));good(controller.set_geometry(geometry()));
    port->partial_present=true;auto error=controller.advance(0,active());
    require(!error&&port->layout&&!port->deadline&&take(controller.snapshot()).needs_reconciliation,
        "A partially applied presentation was not marked uncertain");
    require(!controller.tick(1),"An uncertain presentation accepted another update");
    good(controller.reconcile());require(!port->layout&&!port->deadline&&!take(controller.snapshot()).needs_reconciliation,
        "Partial presentation could not be reconciled");
}
}
int main(){presentation_and_failure();scheduling_and_exceptions();callback_and_lifetime();failed_rollback();timer_delivery();partial_presentation();}
