#include <LVUI/NativeTooltipScheduler.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void cancellation_and_errors(){
    ii::runtime::EventLoop loop;auto scheduler=take(lvui::native::NativeTooltipScheduler::create(loop.sender()));
    require(!scheduler.schedule(0),"Scheduler accepted a deadline without a wake receiver");int calls=0;
    good(scheduler.set_wake([&](std::int64_t now)->ii::core::Result<void>{require(now>=0,"Negative scheduler time");++calls;return {};}));
    good(scheduler.schedule(0));good(scheduler.schedule(0));(void)loop.poll();require(calls==1,"Replacing a deadline delivered an obsolete wake");
    good(scheduler.schedule(0));good(scheduler.schedule(std::nullopt));(void)loop.poll();require(calls==1,"Canceled deadline still delivered");
    good(scheduler.set_wake([](std::int64_t)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"Wake rejected");}));
    good(scheduler.schedule(0));(void)loop.poll();auto failure=take(scheduler.last_failure());
    require(failure&&failure->code==ii::core::ErrorCode::failed,"A failed wake was not recorded");
    good(scheduler.set_wake([](std::int64_t)->ii::core::Result<void>{throw std::runtime_error("Wake threw");}));
    good(scheduler.schedule(0));(void)loop.poll();failure=take(scheduler.last_failure());require(failure&&failure->cause,"A wake exception lost its cause");
    bool wrong_thread=false;std::thread other([&]{auto result=scheduler.schedule(0);wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});other.join();
    require(wrong_thread,"A foreign thread changed the scheduler");
    require(!scheduler.schedule(-1)&&!scheduler.schedule(std::numeric_limits<std::int64_t>::max()),"Invalid scheduler deadline was accepted");
    auto weak=scheduler.weak_schedule();good(scheduler.schedule(0));good(scheduler.retire());(void)loop.poll();
    auto retired=weak(0);require(!retired&&retired.error().code==ii::core::ErrorCode::closed,"Retired scheduler accepted a deadline");
}
void reentry_and_capacity(){
    {
        ii::runtime::EventLoop loop(1);auto scheduler=take(lvui::native::NativeTooltipScheduler::create(loop.sender()));int calls=0;
        good(scheduler.set_wake([&](std::int64_t)->ii::core::Result<void>{++calls;return {};}));good(scheduler.schedule(60000));
        auto receiver=scheduler.set_wake([](std::int64_t)->ii::core::Result<void>{return {};});
        require(!receiver&&receiver.error().code==ii::core::ErrorCode::busy,"A pending deadline changed its wake receiver");
        auto full=scheduler.schedule(0);require(!full&&full.error().code==ii::core::ErrorCode::full,"Full loop did not reject the replacement deadline");
        good(scheduler.schedule(std::nullopt));good(scheduler.schedule(0));(void)loop.poll();require(calls==1,"Scheduler did not recover after capacity was released");
    }
    {
        ii::runtime::EventLoop loop(2);auto scheduler=take(lvui::native::NativeTooltipScheduler::create(loop.sender()));
        auto weak=scheduler.weak_schedule();int calls=0;
        good(scheduler.set_wake([&](std::int64_t)->ii::core::Result<void>{++calls;if(calls==1)return weak(0);return {};}));
        good(scheduler.schedule(0));(void)loop.poll();if(calls==1)(void)loop.poll();
        require(calls==2,"A wake could not safely schedule its successor");
        require(!take(scheduler.last_failure()),"A successful wake retained an earlier failure");
    }
}
void foreign_delivery_and_lifetime(){
    ii::runtime::EventLoop loop;std::function<ii::core::Result<void>(std::optional<std::int64_t>)> weak;int calls=0;
    {
        auto scheduler=take(lvui::native::NativeTooltipScheduler::create(loop.sender()));weak=scheduler.weak_schedule();
        good(scheduler.set_wake([&](std::int64_t)->ii::core::Result<void>{++calls;return {};}));good(scheduler.schedule(0));
        std::thread other([&]{(void)loop.poll();});other.join();auto failure=take(scheduler.last_failure());
        require(failure&&failure->code==ii::core::ErrorCode::wrong_executor&&calls==0,"A foreign delivery reached UI state");
        good(scheduler.schedule(0));
    }
    (void)loop.poll();require(calls==0,"An expired scheduler retained its wake receiver");
    auto expired=weak(0);require(!expired&&expired.error().code==ii::core::ErrorCode::closed,"Expired scheduler accepted a deadline");
}
}
int main(){cancellation_and_errors();reentry_and_capacity();foreign_delivery_and_lifetime();}
