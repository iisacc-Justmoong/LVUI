#include <LVUI/NativeQueryConnections.h>
#include <LVUI/NativeBackendFactories.h>
#include <iiSDKCore/Runtime/QueryStream.h>
#include <chrono>
#include <latch>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
void good(ii::core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
}
int main(){
    using namespace ii::runtime;using namespace lvui::native;
    EventLoop loop(32);auto connections=take(QueryConnections::create(loop));
    auto factories=BackendFactories{}.with_query_connections(connections).with<int>([]{return 1;})
        .with_activation([]{return BackendActivation{};});
    check(factories.query_connections()==connections,"Backend factory composition lost query connections");
    QueryStream<int,int,int> feed([](int selection,int source)->std::optional<int>{
        if(source%2!=selection)return {};return source;
    });
    auto owner=std::make_shared<int>(1);std::vector<int> first,second;
    auto a=take(connections->connect(owner,feed,0,[&](const int& value){first.push_back(value);}));
    auto b=take(connections->connect(owner,feed,1,[&](const int& value){second.push_back(value);}));
    bool rejected_foreign_thread=false;
    std::thread outside([&]{auto result=a->select(1);rejected_foreign_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    outside.join();check(rejected_foreign_thread,"A foreign thread changed a UI query selection");
    EventLoop foreign_loop(8);auto foreign=take(QueryConnections::create(foreign_loop));
    check(!foreign->retire(a)&&take(foreign->close()),"A different host retired this connection");
    a->activate();b->activate();
    std::thread producer([&]{(void)feed.publish(1);(void)feed.publish(2);(void)feed.publish(3);});producer.join();
    check(first.empty()&&second.empty(),"Query events bypassed their UI executor");
    loop.poll({32,1s});check(first==std::vector<int>{2}&&second==std::vector<int>({1,3}),"Selection, fanout or ordering failed");
    good(a->select(1));(void)feed.publish(5);loop.poll({32,1s});
    check(first==std::vector<int>({2,5})&&second==std::vector<int>({1,3,5}),"A live connection did not replace its query");
    good(connections->retire(a));(void)feed.publish(7);loop.poll({32,1s});
    check(first==std::vector<int>({2,5})&&second.back()==7,"A retired receiver got a later event");
    good(connections->retire(b));
    using Feed=QueryStream<int,int,int>;std::shared_ptr<QueryConnection<Feed,int>> self;
    std::function<void(const int&)> empty;
    check(!connections->connect(owner,feed,1,empty),"An empty receiver callback was accepted");
    int moved_value=0;
    auto moved=take(connections->connect(owner,feed,1,[value=std::make_unique<int>(6),&moved_value](const int& event){moved_value=*value+event;}));
    moved->activate();(void)feed.publish(1);loop.poll({32,1s});
    check(moved_value==7,"A move-only receiver did not retain its capture");good(connections->retire(moved));
    auto local=std::make_shared<int>(2);std::weak_ptr<int> weak=local;int calls=0;
    self=take(connections->connect(local,feed,1,[&](const int&){++calls;good(connections->retire(self));local.reset();}));self->activate();
    (void)feed.publish(9);loop.poll({1,1s});
    check(calls==1&&!weak.expired()&&connections->retiring()==1,"Reentrant removal lost its owner or barrier");
    loop.poll({32,1s});check(weak.expired()&&connections->retiring()==0,"Deferred query removal did not finish");
    std::latch entered(1),release(1);
    Feed busy([&](int,int value)->std::optional<int>{entered.count_down();release.wait();return value;});
    auto busy_owner=std::make_shared<int>(3);std::weak_ptr<int> busy_weak=busy_owner;
    auto slow=take(connections->connect(busy_owner,busy,0,[&](const int&){++calls;}));slow->activate();
    std::thread running([&]{(void)busy.publish(11);});entered.wait();
    good(connections->retire(slow));busy_owner.reset();
    check(!busy_weak.expired()&&connections->retiring()==1,"A running selector blocked retirement or lost its owner");
    release.count_down();running.join();good(connections->poll());
    check(busy_weak.expired()&&connections->retiring()==0&&bool(busy.close()),"A completed selector did not release its receiver");
    check(take(connections->close()),"Idle query registry did not close");
    check(bool(feed.close()),"Feed did not close after receiver retirement");

    EventLoop tiny(2);auto bounded=take(QueryConnections::create(tiny));
    Feed queued([](int,int value){return std::optional<int>(value);});auto token=std::make_shared<int>(4);int delivered=0;
    auto waiting=take(bounded->connect(token,queued,0,[&](const int& value){delivered=value;}));waiting->activate();
    check(bool(tiny.post([]{})),"Could not reserve the bounded-loop fixture");
    auto publication=queued.publish(42);check(publication&&publication->dispatch_failed==1&&take(waiting->stats()).pending==1,
        "Executor saturation was not observable");
    auto snapshots=take(bounded->stats());
    check(snapshots.size()==1&&snapshots[0].stats.pending==1&&snapshots[0].stats.dispatch_failed==1,
        "Host could not observe generated connection backpressure");
    tiny.poll({1,1s});auto retries=take(bounded->resume());
    check(retries.size()==1&&bool(retries[0]),"A retained event could not be retried by the host");
    tiny.poll({8,1s});check(delivered==42,"A retried event was not delivered");
    good(bounded->retire(waiting));check(take(bounded->close())&&bool(queued.close()),"Bounded query fixture did not retire");

    EventLoop errors(8);auto observed=take(QueryConnections::create(errors));
    Feed throwing([](int,int value){return std::optional<int>(value);});auto receiver=std::make_shared<int>(5);int recovered=0;
    auto failure=take(observed->connect(receiver,throwing,0,[&](const int& value){
        if(value==1)throw std::runtime_error("receiver failed");recovered=value;
    }));failure->activate();
    (void)throwing.publish(1);(void)throwing.publish(2);
    bool reported=false;try{errors.poll({8,1s});}catch(const std::runtime_error&){reported=true;}
    auto failures=take(observed->stats());
    check(reported&&failures.size()==1&&failures[0].stats.callback_failed==1&&bool(failures[0].failure),
        "Receiver failure did not reach the host diagnostics");
    auto recovery=take(observed->resume());check(recovery.size()==1&&bool(recovery[0]),"Failed receiver could not resume");
    errors.poll({8,1s});check(recovered==2,"A callback failure lost later admitted work");
    good(observed->retire(failure));check(take(observed->close())&&bool(throwing.close()),"Failed receiver did not retire");

    EventLoop stopped(8);auto shutdown=take(QueryConnections::create(stopped));
    std::latch shutdown_entered(1),shutdown_release(1);
    Feed in_flight([&](int,int value)->std::optional<int>{shutdown_entered.count_down();shutdown_release.wait();return value;});
    auto retained=std::make_shared<int>(6);std::weak_ptr<int> retained_weak=retained;
    auto pending=take(shutdown->connect(retained,in_flight,0,[](const int&){}));pending->activate();
    std::thread stopping_worker([&]{(void)in_flight.publish(1);});shutdown_entered.wait();
    stopped.request_stop();auto retired=shutdown->retire(pending);retained.reset();
    check(!retired&&retired.error().code==ii::core::ErrorCode::closed&&!retained_weak.expired(),
          "Stopped-loop cleanup did not report its deferred ownership");
    shutdown_release.count_down();stopping_worker.join();good(shutdown->poll());
    check(retained_weak.expired()&&take(shutdown->close())&&bool(in_flight.close()),
          "Stopped-loop host polling did not complete retirement");
}
