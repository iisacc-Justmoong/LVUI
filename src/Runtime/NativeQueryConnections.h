#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/QueryStream.h>
#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace lvui::native {
class QueryConnections;
struct QueryConnectionStatus final {
    ii::runtime::QuerySubscriptionStats stats;
    std::exception_ptr failure;
};
namespace query_connection_detail {
struct Record final {
    bool retiring=false;
    std::weak_ptr<QueryConnections> owner;
    std::function<void()> retire;
    std::function<ii::runtime::ThreadResult()> finish;
    std::function<QueryConnectionStatus()> status;
    std::function<ii::runtime::Submission()> resume;
};
}

template<class Stream,class Selection> class QueryConnection;

// The host retains this owner until every retired stream has crossed Core's
// nonblocking removal barrier. One reserved signal retries without polling.
class QueryConnections final : public std::enable_shared_from_this<QueryConnections> {
public:
    [[nodiscard]] static ii::core::Result<std::shared_ptr<QueryConnections>> create(ii::runtime::EventLoop& loop,std::size_t capacity=4096){
        try{
            if(!capacity||capacity>65536)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid query connection capacity");
            auto value=std::shared_ptr<QueryConnections>(new QueryConnections(loop,capacity));
            auto signal=value->sender_.on_signal([weak=std::weak_ptr(value)]{if(auto owner=weak.lock())(void)owner->collect(true);});
            if(!signal)return ii::core::fail(signal.error==ii::runtime::SubmitError::full?ii::core::ErrorCode::full:ii::core::ErrorCode::closed,"Query cleanup reservation was rejected");
            value->signal_=signal.token;return value;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Query connection owner creation failed",std::current_exception());}
    }
    ~QueryConnections(){if(!records_.empty())std::terminate();(void)signal_.cancel();}
    QueryConnections(const QueryConnections&)=delete;
    QueryConnections& operator=(const QueryConnections&)=delete;

    template<class Owner,class Stream,class Selection,class Callback>
    [[nodiscard]] auto connect(const std::shared_ptr<Owner>& owner,Stream& stream,Selection selection,Callback callback,
            ii::runtime::QuerySubscriptionOptions options={})
        -> ii::core::Result<std::shared_ptr<QueryConnection<Stream,std::remove_cvref_t<Selection>>>>;

    template<class Stream,class Selection>
    [[nodiscard]] ii::core::Result<void> retire(const std::shared_ptr<QueryConnection<Stream,Selection>>& connection){
        if(auto checked=check();!checked)return checked;
        if(!connection)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Missing query connection");
        auto record=connection->record_;
        if(!record||record->owner.lock().get()!=this)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Query connection has a different owner");
        if(std::ranges::find(records_,record)==records_.end()){
            if(record->retiring)return {};
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Query connection has a different owner");
        }
        if(!record->retiring){record->retiring=true;record->retire();}
        return collect(false);
    }
    // Returns false while a producer, callback, or its captures still retire.
    [[nodiscard]] ii::core::Result<bool> close(){
        if(auto checked=check();!checked)return std::unexpected(checked.error());
        accepting_=false;
        auto current=records_;
        for(auto& record:current)if(!record->retiring){record->retiring=true;record->retire();}
        if(auto result=collect(false);!result)return std::unexpected(result.error());
        return records_.empty();
    }
    [[nodiscard]] ii::core::Result<void> poll(){if(auto checked=check();!checked)return checked;return collect(false);}
    // Active connections are returned in registration order; snapshots are not
    // stable identities and may change after a View is replaced.
    [[nodiscard]] ii::core::Result<std::vector<QueryConnectionStatus>> stats()const{
        if(auto checked=check();!checked)return std::unexpected(checked.error());
        try{std::vector<QueryConnectionStatus> values;values.reserve(records_.size());
            for(const auto& record:records_)if(!record->retiring)values.push_back(record->status());
            return values;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Query statistics failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::vector<ii::runtime::Submission>> resume(){
        if(auto checked=check();!checked)return std::unexpected(checked.error());
        try{std::vector<ii::runtime::Submission> values;values.reserve(records_.size());
            for(const auto& record:records_)if(!record->retiring)values.push_back(record->resume());
            return values;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Query retry failed",std::current_exception());}
    }
    [[nodiscard]] std::size_t retiring()const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("Query connections require their UI thread");
        return std::ranges::count_if(records_,[](const auto& value){return value->retiring;});
    }
    [[nodiscard]] std::size_t size()const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("Query connections require their UI thread");return records_.size();
    }
    [[nodiscard]] bool uses(const ii::runtime::EventLoop& loop)const noexcept{return loop_==&loop&&signal_.pending();}
private:
    QueryConnections(ii::runtime::EventLoop& loop,std::size_t capacity)
        :sender_(loop.sender()),loop_(&loop),capacity_(capacity){records_.reserve(capacity);}
    [[nodiscard]] ii::core::Result<void> check()const{
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Query connections require their UI thread");
        return {};
    }
    [[nodiscard]] ii::core::Result<void> collect(bool retry){
        if(auto checked=check();!checked)return checked;
        if(collecting_)return {};
        collecting_=true;struct Guard final{bool& value;~Guard(){value=false;}}guard{collecting_};
        for(std::size_t index=0;index<records_.size();){
            auto record=records_[index];if(!record->retiring){++index;continue;}
            auto result=record->finish();
            if(result){
                auto found=std::ranges::find(records_,record);
                if(found!=records_.end())records_.erase(found);
                continue;
            }
            if(result.error().code!=ii::runtime::ThreadErrorCode::self_join&&result.error().code!=ii::runtime::ThreadErrorCode::would_block)
                return ii::core::fail(ii::core::ErrorCode::failed,"Query subscription retirement failed",result.error().cause);
            ++index;
        }
        if(retiring()){
            keep_alive_=shared_from_this();
            const auto delay=retry?std::chrono::milliseconds(1):std::chrono::milliseconds(0);
            if(!sender_.signal(signal_,ii::runtime::Clock::now()+delay))
                return ii::core::fail(ii::core::ErrorCode::closed,"Query retirement requires a live UI event loop");
        }else{
            keep_alive_.reset();
            if(!accepting_)(void)signal_.cancel();
        }
        return {};
    }
    ii::runtime::EventLoop::Sender sender_;
    const ii::runtime::EventLoop* loop_;
    ii::runtime::TaskToken signal_;
    const std::thread::id owner_=std::this_thread::get_id();
    const std::size_t capacity_;
    std::vector<std::shared_ptr<query_connection_detail::Record>> records_;
    std::shared_ptr<QueryConnections> keep_alive_;
    bool accepting_=true,collecting_=false;
};

template<class Stream,class Selection> class QueryConnection final {
    friend class QueryConnections;
    using Event=typename Stream::Event;
    using Value=std::remove_cvref_t<decltype(std::declval<Event>().value)>;
    struct State final {
        std::weak_ptr<void> owner;
        std::shared_ptr<void> retiring_owner;
        std::optional<typename Stream::Subscription> subscription;
        std::function<void(const Value&)> callback;
        const std::thread::id thread=std::this_thread::get_id();
        bool active=false,retired=false;
    };
    explicit QueryConnection(std::shared_ptr<State> state,std::shared_ptr<query_connection_detail::Record> record)
        :state_(std::move(state)),record_(std::move(record)){}
public:
    void activate(){
        if(state_->thread!=std::this_thread::get_id())throw std::logic_error("Query activation requires its UI thread");
        if(!state_->retired)state_->active=true;
    }
    [[nodiscard]] ii::core::Result<void> select(const Selection& value){
        if(state_->thread!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Query selection requires its UI thread");
        if(state_->retired||!state_->subscription)return ii::core::fail(ii::core::ErrorCode::closed,"Query connection is retired");
        auto result=state_->subscription->queries().send(value);
        if(!result)return ii::core::fail(ii::core::ErrorCode::failed,"Query selection delivery failed",result.error().cause);
        if(!*result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Query selection was rejected");
        return {};
    }
    [[nodiscard]] ii::core::Result<ii::runtime::QuerySubscriptionStats> stats()const{
        if(state_->thread!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Query statistics require their UI thread");
        if(!state_->subscription)return ii::core::fail(ii::core::ErrorCode::closed,"Query connection is retired");
        return state_->subscription->stats();
    }
    [[nodiscard]] ii::core::Result<ii::runtime::Submission> resume()const{
        if(state_->thread!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Query retry requires its UI thread");
        if(state_->retired||!state_->subscription)return ii::core::fail(ii::core::ErrorCode::closed,"Query connection is retired");
        return state_->subscription->resume();
    }
private:
    std::shared_ptr<State> state_;
    std::shared_ptr<query_connection_detail::Record> record_;
};

template<class Owner,class Stream,class Selection,class Callback>
auto QueryConnections::connect(const std::shared_ptr<Owner>& owner,Stream& stream,Selection selection,Callback callback,
        ii::runtime::QuerySubscriptionOptions options)
    -> ii::core::Result<std::shared_ptr<QueryConnection<Stream,std::remove_cvref_t<Selection>>>>{
    using Connection=QueryConnection<Stream,std::remove_cvref_t<Selection>>;
    try{
        if(auto checked=check();!checked)return std::unexpected(checked.error());
        if(!accepting_)return ii::core::fail(ii::core::ErrorCode::closed,"Query connection owner is closed");
        if(!owner)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Query receiver requires an owner");
        if(records_.size()==capacity_)return ii::core::fail(ii::core::ErrorCode::full,"Query connection capacity exhausted");
        if constexpr(requires{static_cast<bool>(callback);})
            if(!static_cast<bool>(callback))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Query receiver requires a callback");
        auto state=std::make_shared<typename Connection::State>();state->owner=owner;
        auto owned_callback=std::make_shared<Callback>(std::move(callback));
        state->callback=[owned_callback](const auto& value){std::invoke(*owned_callback,value);};
        auto record=std::make_shared<query_connection_detail::Record>();
        record->owner=weak_from_this();
        record->retire=[state]{state->active=false;state->retired=true;state->retiring_owner=state->owner.lock();};
        record->finish=[state]()->ii::runtime::ThreadResult{
            if(state->subscription){auto result=state->subscription->try_close();if(!result)return result;state->subscription.reset();}
            state->callback={};state->retiring_owner.reset();return {};
        };
        record->status=[state]{return QueryConnectionStatus{state->subscription->stats(),state->subscription->failure()};};
        record->resume=[state]{return state->subscription->resume();};
        auto result=std::shared_ptr<Connection>(new Connection(state,record));
        records_.push_back(record);
        struct Rollback final {
            std::vector<std::shared_ptr<query_connection_detail::Record>>& records;
            std::shared_ptr<query_connection_detail::Record> record;
            bool committed=false;
            ~Rollback(){if(!committed)std::erase(records,record);}
        } rollback{records_,record};
        auto receiver=stream.subscribe_on(sender_,std::move(selection),[weak=std::weak_ptr(state)](typename Stream::Event event){
            auto state=weak.lock();if(!state||!state->active||state->retired)return;
            if(state->thread!=std::this_thread::get_id())throw std::logic_error("Query event was dispatched outside its UI thread");
            auto owner=state->owner.lock();if(!owner||!state->subscription||event.revision!=state->subscription->stats().revision)return;
            state->callback(event.value);
        },options);
        if(!receiver)return ii::core::fail(receiver.error().code==ii::runtime::QueryStreamErrorCode::exhausted?ii::core::ErrorCode::full:ii::core::ErrorCode::invalid_argument,"Query stream subscription was rejected");
        state->subscription.emplace(std::move(*receiver));
        rollback.committed=true;
        return result;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Query connection failed",std::current_exception());}
}
} // namespace lvui::native
