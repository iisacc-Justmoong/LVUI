#pragma once
#include "NativeBackendFactories.h"
#include <iiSDKCore/Runtime/Runtime.h>
#include <iiSDKCore/Runtime/TaskScope.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <stop_token>

namespace lvui::native {
namespace work_detail {struct State;}
class WorkScope;
class WorkGroup final {
public:
    ~WorkGroup();
    WorkGroup(const WorkGroup&)=delete;WorkGroup& operator=(const WorkGroup&)=delete;
    [[nodiscard]] std::shared_ptr<WorkScope> scope(std::string_view binding);
    void attach(std::weak_ptr<void>,std::function<void()> changed);
    void retire(std::shared_ptr<void> owner);
private:
    friend class WorkRuntime;
    explicit WorkGroup(std::weak_ptr<work_detail::State>);
    void check()const;
    std::thread::id thread_=std::this_thread::get_id();
    std::weak_ptr<work_detail::State> runtime_;
    std::map<std::string,std::shared_ptr<WorkScope>,std::less<>> scopes_;
    std::weak_ptr<void> owner_;
    std::function<void()> changed_;
    bool attached_=false,closed_=false;
};
struct WorkOptions final {std::size_t maximum_scopes=4096;ii::runtime::Clock::duration retirement_interval=std::chrono::milliseconds(1);};
struct WorkStats final {std::size_t scopes=0,retiring=0;ii::runtime::SubmitError retirement_dispatch=ii::runtime::SubmitError::none;};
class WorkScope final:public std::enable_shared_from_this<WorkScope> {
public:
    ~WorkScope();
    [[nodiscard]] ii::runtime::Submission post(std::function<void(std::stop_token)>,ii::runtime::Priority=ii::runtime::Priority::normal);
    [[nodiscard]] ii::runtime::Submission deliver(ii::runtime::Task,ii::runtime::Priority=ii::runtime::Priority::normal);
    template<class F>requires std::invocable<F&,std::stop_token> && std::same_as<std::invoke_result_t<F&,std::stop_token>,void> && (!std::same_as<std::remove_cvref_t<F>,std::function<void(std::stop_token)>>)
    [[nodiscard]] ii::runtime::Submission post(F task,ii::runtime::Priority priority=ii::runtime::Priority::normal){
        auto owned=std::make_shared<F>(std::move(task));
        return post(std::function<void(std::stop_token)>{[owned](auto stop){(void)std::invoke(*owned,stop);}},priority);
    }
    template<class F>requires std::invocable<F&,std::stop_token> && (!std::same_as<std::invoke_result_t<F&,std::stop_token>,void>)
    ii::runtime::Submission post(F,ii::runtime::Priority=ii::runtime::Priority::normal)=delete;
    template<class F>requires std::invocable<F&> && std::same_as<std::invoke_result_t<F&>,void> && (!std::same_as<std::remove_cvref_t<F>,ii::runtime::Task>)
    [[nodiscard]] ii::runtime::Submission deliver(F task,ii::runtime::Priority priority=ii::runtime::Priority::normal){
        auto owned=std::make_shared<F>(std::move(task));
        return deliver(ii::runtime::Task{[owned]{(void)std::invoke(*owned);}},priority);
    }
    template<class F>requires std::invocable<F&> && (!std::same_as<std::invoke_result_t<F&>,void>)
    ii::runtime::Submission deliver(F,ii::runtime::Priority=ii::runtime::Priority::normal)=delete;
    [[nodiscard]] std::optional<ii::core::Error> failure()const;
    [[nodiscard]] bool closing()const noexcept{return closed_.load();}
private:
    friend struct work_detail::State;
    friend class WorkGroup;
    void attach(std::weak_ptr<void>,std::function<void()> changed);
    void retire(std::shared_ptr<void> owner);
    explicit WorkScope(const std::shared_ptr<work_detail::State>&);
    bool finish(bool wait);
    void owner()const;
    void failed(std::exception_ptr);
    std::thread::id thread_=std::this_thread::get_id();
    std::weak_ptr<work_detail::State> runtime_;
    ii::runtime::SerialQueue queue_;
    ii::runtime::TaskScope work_,deliveries_;
    std::stop_source stop_;
    std::atomic_bool attached_=false,closed_=false;
    std::weak_ptr<void> owner_;
    std::shared_ptr<void> retiring_owner_;
    std::function<void()> changed_;
    mutable std::mutex error_mutex_;
    std::optional<ii::core::Error> error_;
};
// The application owns this coordinator and its borrowed persistent Runtime.
// View removal is nonblocking; join is the application's final shutdown barrier.
class WorkRuntime final {
public:
    WorkRuntime(ii::runtime::Runtime&,ii::runtime::EventLoop::Sender,WorkOptions={});
    ~WorkRuntime();
    WorkRuntime(const WorkRuntime&)=delete;WorkRuntime& operator=(const WorkRuntime&)=delete;
    [[nodiscard]] BackendFactories factories(BackendFactories={})const;
    [[nodiscard]] static std::shared_ptr<WorkScope> scope(const BackendRequest&);
    void poll();
    [[nodiscard]] bool close();
    [[nodiscard]] ii::runtime::ThreadResult join();
    [[nodiscard]] WorkStats stats()const;
private:
    std::shared_ptr<work_detail::State> state_;
};
} // namespace lvui::native
