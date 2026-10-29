#include <LVUI/NativeWork.h>
#include <algorithm>
#include <vector>

namespace lvui::native {
using namespace ii::runtime;
namespace work_detail {
struct State final:std::enable_shared_from_this<State> {
    Runtime& workers;EventLoop::Sender ui;WorkOptions options;
    std::thread::id owner=std::this_thread::get_id();
    std::vector<std::weak_ptr<WorkScope>> scopes;
    std::vector<std::shared_ptr<WorkScope>> retiring;
    std::vector<std::shared_ptr<void>> shutdown_owners;
    TaskToken timer;SubmitError dispatch=SubmitError::none;bool accepting=true,polling=false;
    State(Runtime& workers,EventLoop::Sender ui,WorkOptions options):workers(workers),ui(std::move(ui)),options(options){
        if(!options.maximum_scopes||options.maximum_scopes>65536||options.retirement_interval<=Clock::duration::zero())throw std::invalid_argument("Invalid activation work bounds");
        scopes.reserve(options.maximum_scopes);retiring.reserve(options.maximum_scopes);shutdown_owners.reserve(options.maximum_scopes);
    }
    void check()const{if(owner!=std::this_thread::get_id())throw std::logic_error("WorkRuntime requires its owner UI thread");}
    std::shared_ptr<WorkScope> create(){
        check();if(!accepting)throw std::logic_error("WorkRuntime closed");
        std::erase_if(scopes,[](const auto& item){return item.expired();});
        if(scopes.size()==options.maximum_scopes)throw std::length_error("Activation work scope capacity exhausted");
        auto result=std::shared_ptr<WorkScope>(new WorkScope(shared_from_this()));scopes.push_back(result);return result;
    }
    void schedule(){
        if(retiring.empty()||timer.pending())return;
        try{auto pending=ui.after(options.retirement_interval,[weak=weak_from_this()]{if(auto state=weak.lock()){state->timer={};state->collect(false);}});
            dispatch=pending.error;timer=pending.token;
        }catch(...){dispatch=SubmitError::overloaded;}
    }
    void collect(bool wait){
        check();if(polling)return;polling=true;
        struct Reset final{bool& value;~Reset(){value=false;}}reset{polling};
        for(std::size_t index=0;index<retiring.size();){auto context=retiring[index];
            if(context->finish(wait))retiring.erase(retiring.begin()+std::ptrdiff_t(index));else ++index;
        }
        if(!wait)schedule();
    }
    void shutdown(){
        check();if(!accepting)return;accepting=false;
        for(const auto& weak:scopes)if(auto context=weak.lock())if(context->attached_&&!context->closed_)shutdown_owners.push_back(context->owner_.lock());
        for(const auto& weak:scopes)if(auto context=weak.lock())if(context->attached_&&!context->closed_)context->retire(context->owner_.lock());
        shutdown_owners.clear();
    }
};
}
WorkGroup::WorkGroup(std::weak_ptr<work_detail::State> runtime):runtime_(std::move(runtime)){}
WorkGroup::~WorkGroup()=default;
void WorkGroup::check()const{if(thread_!=std::this_thread::get_id())throw std::logic_error("WorkGroup requires its owner UI thread");}
std::shared_ptr<WorkScope> WorkGroup::scope(std::string_view binding){
    check();if(closed_||binding.empty()||binding.size()>1024)throw std::logic_error("Invalid or retired backend work binding");
    if(auto found=scopes_.find(binding);found!=scopes_.end())return found->second;
    auto runtime=runtime_.lock();if(!runtime)throw std::logic_error("WorkRuntime expired");auto context=runtime->create();
    if(attached_)context->attach(owner_,changed_);
    scopes_.emplace(binding,context);return context;
}
void WorkGroup::attach(std::weak_ptr<void> value,std::function<void()> changed){
    check();auto runtime=runtime_.lock();if(!runtime||!runtime->accepting||value.expired()||attached_||closed_)throw std::logic_error("WorkGroup cannot attach its owner");
    owner_=std::move(value);changed_=std::move(changed);
    for(const auto& [name,scope]:scopes_){(void)name;scope->attach(owner_,changed_);}attached_=true;
}
void WorkGroup::retire(std::shared_ptr<void> value){
    check();if(closed_)return;
    if(owner_.lock()!=value)throw std::logic_error("WorkGroup retirement requires its original owner");
    closed_=true;
    for(const auto& [name,scope]:scopes_){(void)name;scope->retire(value);}changed_={};
}
WorkScope::WorkScope(const std::shared_ptr<work_detail::State>& runtime):runtime_(runtime),queue_(runtime->workers){}
WorkScope::~WorkScope()=default;
void WorkScope::owner()const{if(thread_!=std::this_thread::get_id())throw std::logic_error("WorkScope requires its owner UI thread");}
void WorkScope::attach(std::weak_ptr<void> value,std::function<void()> changed){
    owner();auto runtime=runtime_.lock();if(!runtime||!runtime->accepting||value.expired()||attached_||closed_)throw std::logic_error("WorkScope cannot attach its owner");
    owner_=std::move(value);changed_=std::move(changed);attached_=true;
}
void WorkScope::retire(std::shared_ptr<void> value){
    owner();if(closed_.exchange(true))return;
    auto runtime=runtime_.lock();if(!runtime)throw std::logic_error("WorkRuntime must outlive its activation");
    retiring_owner_=value;runtime->retiring.push_back(shared_from_this());
    stop_.request_stop();work_.request_close();deliveries_.request_close();runtime->schedule();
}
void WorkScope::failed(std::exception_ptr cause){
    std::string message="Native activation work failed";
    try{std::rethrow_exception(cause);}catch(const std::exception& error){message+=": ";message+=error.what();}catch(...){}
    std::lock_guard lock(error_mutex_);error_={ii::core::ErrorCode::failed,std::move(message),std::move(cause)};
}
Submission WorkScope::post(std::function<void(std::stop_token)> task,Priority priority){
    owner();if(!task)return {{},SubmitError::invalid};
    auto runtime=runtime_.lock();auto owning=owner_.lock();
    if(!runtime||!runtime->accepting||!attached_||closed_||!owning)return {{},SubmitError::stopped};
    return work_.post(queue_,[owning=std::move(owning),weak=weak_from_this(),task=std::move(task),stop=stop_.get_token()]{
        try{task(stop);}catch(...){if(auto context=weak.lock()){context->failed(std::current_exception());(void)context->deliver([]{});}}
    },priority);
}
Submission WorkScope::deliver(Task task,Priority priority){
    if(!task)return {{},SubmitError::invalid};
    if(!attached_||closed_)return {{},SubmitError::stopped};
    auto runtime=runtime_.lock();if(!runtime||owner_.expired())return {{},SubmitError::stopped};
    auto ui=runtime->ui;
    return deliveries_.post(ui,[receiver=owner_,weak=weak_from_this(),task=std::move(task)]{
        auto context=weak.lock();if(!context||context->closed_)return;
        try{context->owner();}catch(...){context->failed(std::current_exception());return;}
        auto owning=receiver.lock();if(!owning)return;
        try{task();}catch(...){context->failed(std::current_exception());}
        try{if(!context->closed_&&context->changed_)context->changed_();}catch(...){context->failed(std::current_exception());}
    },priority);
}
std::optional<ii::core::Error> WorkScope::failure()const{std::lock_guard lock(error_mutex_);return error_;}
bool WorkScope::finish(bool wait){
    owner();auto work=wait?work_.close():work_.try_close();auto deliveries=wait?deliveries_.close():deliveries_.try_close();
    const auto ready=[](const auto& result){return bool(result)||result.error().code==ThreadErrorCode::work_failed;};
    if(!ready(work)||!ready(deliveries))return false;
    changed_={};
    // Preserve the diagnostic text without retaining arbitrary exception-owned
    // receiver captures past final retirement. External snapshots own theirs.
    std::exception_ptr retired_error;
    {std::lock_guard lock(error_mutex_);if(error_)retired_error=std::exchange(error_->cause,{});}
    retired_error={};retiring_owner_.reset();return true;
}
WorkRuntime::WorkRuntime(Runtime& workers,EventLoop::Sender ui,WorkOptions options):state_(std::make_shared<work_detail::State>(workers,std::move(ui),options)){}
WorkRuntime::~WorkRuntime(){if(!join())std::terminate();}
BackendFactories WorkRuntime::factories(BackendFactories base)const{
    state_->check();return base.with_activation([weak=std::weak_ptr(state_)]{
        auto runtime=weak.lock();if(!runtime)throw std::logic_error("WorkRuntime expired");
        runtime->check();if(!runtime->accepting)throw std::logic_error("WorkRuntime closed");
        return BackendActivation::from(std::shared_ptr<WorkGroup>(new WorkGroup(runtime)));
    });
}
std::shared_ptr<WorkScope> WorkRuntime::scope(const BackendRequest& request){
    auto group=request.activation.get<WorkGroup>();if(!group)throw std::logic_error("Missing activation work group");return group->scope(request.binding);
}
void WorkRuntime::poll(){state_->check();(void)state_->timer.cancel();state_->timer={};state_->collect(false);}
bool WorkRuntime::close(){state_->shutdown();poll();return state_->retiring.empty();}
ThreadResult WorkRuntime::join(){
    state_->shutdown();(void)state_->timer.cancel();state_->timer={};state_->collect(true);
    if(!state_->retiring.empty())return std::unexpected(ThreadError{ThreadErrorCode::would_block,{}});return {};
}
WorkStats WorkRuntime::stats()const{
    state_->check();return {std::size_t(std::ranges::count_if(state_->scopes,[](const auto& scope){return !scope.expired();})),state_->retiring.size(),state_->dispatch};
}
} // namespace lvui::native
