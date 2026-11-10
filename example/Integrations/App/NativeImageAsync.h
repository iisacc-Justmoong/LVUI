#pragma once

#include "NativeImageLoader.h"
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/Runtime.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <list>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace lvui::native::detail {
// One reserved signal slot per loader avoids losing a final notification when
// the loop's other slots fill between a worker finishing and its delivery.
// Workers own only their inputs and Job, never the loader or the UI objects.
class ImageWorkQueue final:public std::enable_shared_from_this<ImageWorkQueue> {
    struct Job final {
        std::atomic<bool> cancelled=false;
        std::mutex mutex;
        bool finished=false,started=false,priority=false;
        ii::runtime::TaskToken token;
        ii::runtime::EventLoop::Sender sender;
        ii::runtime::TaskToken completion_signal;
        std::function<void()> work;
        std::function<void()> completion;
        void publish()noexcept{
            ii::runtime::TaskToken signal;
            {std::lock_guard lock(mutex);if(finished)return;finished=true;signal=completion_signal;}
            (void)sender.signal(signal,ii::runtime::Clock::now());
        }
    };
    struct Ticket final {
        std::shared_ptr<Job> job;
        std::function<void()> work;
        ~Ticket(){job->publish();}
        void run()noexcept{
            if(!job->cancelled.load(std::memory_order_acquire)){
                try{work();}catch(...){/* An absent result becomes a worker failure. */}
            }
            job->publish();
        }
    };
    struct Handle final:ImageRequest {
        std::weak_ptr<ImageWorkQueue> owner;
        std::shared_ptr<Job> job;
        Handle(std::weak_ptr<ImageWorkQueue> owner_,std::shared_ptr<Job> job_)
            :owner(std::move(owner_)),job(std::move(job_)){}
        ~Handle()override{cancel();}
        void cancel()noexcept override{
            if(!job)return;
            if(auto queue=owner.lock())queue->cancel(job);
            job.reset();
        }
    };
    ii::runtime::EventLoop& loop_;
    ii::runtime::EventLoop::Sender sender_;
    ii::runtime::Runtime& runtime_;
    const std::size_t maximum_;
    std::list<std::shared_ptr<Job>> jobs_;
    ii::runtime::TaskToken completion_signal_;
    std::exception_ptr failure_;
    std::size_t running_=0;
    bool signal_active_=false,closed_=false,draining_=false;

    bool ensure_signal(){
        if(signal_active_)return true;
        auto submitted=sender_.on_signal([weak=weak_from_this()]{if(auto self=weak.lock())self->drain();},ii::runtime::Priority::background);
        if(!submitted)return false;
        completion_signal_=submitted.token;signal_active_=true;
        // An observer can retire a signal while cancelled workers are still
        // running. Rebind those workers without losing an already-finished job.
        for(const auto& job:jobs_){
            bool finished=false;
            {std::lock_guard lock(job->mutex);job->completion_signal=completion_signal_;finished=job->finished;}
            if(finished)(void)sender_.signal(completion_signal_,ii::runtime::Clock::now());
        }
        return true;
    }
    void stop_signal()noexcept{
        if(!signal_active_)return;
        signal_active_=false;(void)completion_signal_.cancel();completion_signal_={};
    }
    void cancel(const std::shared_ptr<Job>& job)noexcept{
        if(!job)return;
        job->cancelled.store(true,std::memory_order_release);
        job->completion={};
        if(job->started)(void)job->token.cancel();
        else jobs_.remove(job);
        if(jobs_.empty()&&!draining_)stop_signal();
    }
    void launch(){
        const auto parallel=std::min<std::size_t>(maximum_,2);
        while(running_<parallel){
            const auto available=[](const auto& job){return !job->started&&!job->cancelled.load(std::memory_order_acquire);};
            auto next=std::ranges::find_if(jobs_,[&](const auto& job){return available(job)&&job->priority;});
            if(next==jobs_.end())next=std::ranges::find_if(jobs_,available);
            if(next==jobs_.end())return;
            const auto job=*next;job->started=true;++running_;
            auto ticket=std::make_shared<Ticket>();ticket->job=job;ticket->work=std::move(job->work);
            auto submitted=runtime_.post([ticket]{ticket->run();},ii::runtime::Priority::background);
            if(submitted)job->token=submitted.token;
            else job->publish();
        }
    }
    void drain(){
        draining_=true;
        struct Guard final{ImageWorkQueue& queue;~Guard(){queue.draining_=false;if(queue.jobs_.empty())queue.stop_signal();}}guard{*this};
        const std::vector<std::shared_ptr<Job>> snapshot(jobs_.begin(),jobs_.end());
        for(const auto& job:snapshot){
            if(std::ranges::find(jobs_,job)==jobs_.end())continue;
            bool ready=false;{std::lock_guard lock(job->mutex);ready=job->finished;}
            if(!ready)continue;
            jobs_.remove(job);if(job->started)--running_;auto completion=std::move(job->completion);
            if(!job->cancelled.load(std::memory_order_acquire)&&completion){
                try{completion();}catch(...){if(!failure_)failure_=std::current_exception();}
            }
        }
        launch();
        if(failure_){
            auto failure=failure_;
            auto submitted=sender_.post([failure]{std::rethrow_exception(failure);});
            if(submitted)failure_={};
            else if(std::ranges::none_of(jobs_,[](const auto& job){return !job->cancelled.load(std::memory_order_acquire);})){
                // This signal slot retires as the loop propagates the exception.
                // Cancelled workers still occupy their slots until they end;
                // a later submission reserves a fresh signal slot to reap them.
                failure_={};signal_active_=false;completion_signal_={};std::rethrow_exception(failure);
            }
        }
    }
public:
    ImageWorkQueue(ii::runtime::EventLoop& loop,ii::runtime::Runtime& runtime,std::size_t maximum)
        :loop_(loop),sender_(loop.sender()),runtime_(runtime),maximum_(maximum){}
    ~ImageWorkQueue(){
        closed_=true;stop_signal();
        auto jobs=std::move(jobs_);
        for(const auto& job:jobs){job->cancelled.store(true,std::memory_order_release);job->completion={};(void)job->token.cancel();}
    }
    [[nodiscard]] bool loop_available()const{return loop_.state()!=ii::runtime::LoopState::stopped;}
    template<class T>
    ii::core::Result<std::shared_ptr<ImageRequest>> submit(
        std::function<ii::core::Result<T>()> work,std::function<void(ii::core::Result<T>)> completion,bool priority=false){
        using ii::core::ErrorCode;
        if(closed_)return ii::core::fail(ErrorCode::closed,"Image work queue is closed");
        if(!loop_available())return ii::core::fail(ErrorCode::closed,"Image owner loop is stopped");
        if(jobs_.size()>=maximum_)return ii::core::fail(ErrorCode::full,"Image work queue is full");
        if(!ensure_signal())return ii::core::fail(ErrorCode::full,"Image owner loop cannot admit completion");
        auto job=std::make_shared<Job>();job->priority=priority;
        job->sender=sender_;job->completion_signal=completion_signal_;
        auto value=std::make_shared<std::optional<ii::core::Result<T>>>();
        job->completion=[value,completion=std::move(completion)]()mutable{
            if(*value)completion(std::move(**value));
            else completion(ii::core::fail(ErrorCode::cancelled,"Image worker stopped before completion"));
        };
        job->work=[value,work=std::move(work)]()mutable{
            try{value->emplace(work());}
            catch(const std::exception& error){value->emplace(ii::core::fail(ErrorCode::failed,error.what(),std::current_exception()));}
            catch(...){value->emplace(ii::core::fail(ErrorCode::failed,"Image worker threw",std::current_exception()));}
        };
        jobs_.push_back(job);
        launch();
        return std::shared_ptr<ImageRequest>(std::make_shared<Handle>(weak_from_this(),std::move(job)));
    }
};

[[nodiscard]] std::shared_ptr<ImageResourceSource> native_async_image_source(
    ii::runtime::EventLoop&,std::shared_ptr<ImageWorkQueue>,ImageSourceOptions);
} // namespace lvui::native::detail
