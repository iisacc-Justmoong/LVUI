#pragma once
#include "NavigationHistory.h"
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lvui {
struct PageRoute final {std::string pattern;std::shared_ptr<const NavigationPage> page;};

// The presenter builds a candidate page and calls commit exactly once before
// publishing it. It must return the commit error and must not retain it as an
// asynchronous operation. No host or widget type leaks into this interface.
class NavigationPresenter {
public:
    virtual ~NavigationPresenter()=default;
    [[nodiscard]] virtual ii::core::Result<void> present(const NavigationEntry&,
        std::function<ii::core::Result<bool>()> commit)=0;
};
struct PageRouterState final {
    NavigationHistoryState history;
    std::size_t routes=0;
    bool presenter_attached=false;
};

class PageRouter final:public NavigationTarget,public std::enable_shared_from_this<PageRouter> {
    // Kept alive until all other members are destroyed so a page destructor
    // cannot observe partially destroyed routing state.
    bool active_=false;
    std::thread::id owner_=std::this_thread::get_id();
    RouteResolver resolver_;
    NavigationHistory history_;
    std::vector<PageRoute> routes_;
    std::shared_ptr<const NavigationPage> fallback_;
    std::weak_ptr<NavigationPresenter> presenter_;
    struct Guard final {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}};
    struct CommitLease final {
        std::weak_ptr<PageRouter> owner;
        std::optional<NavigationPlan> plan;
        std::atomic_bool active{true},used{false},committed{false};
        CommitLease(std::weak_ptr<PageRouter> source,std::optional<NavigationPlan> value):owner(std::move(source)),plan(std::move(value)){}
    };
    PageRouter()=default;
    [[nodiscard]] ii::core::Result<void> ready()const {
        if(std::this_thread::get_id()!=owner_)return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Page router requires its owner thread");
        if(active_)return ii::core::fail(ii::core::ErrorCode::busy,"Page router is changing");return {};
    }
    [[nodiscard]] ii::core::Result<bool> present_entry(const std::shared_ptr<NavigationPresenter>& presenter,
            const NavigationEntry& entry,std::optional<NavigationPlan> plan){
        if(!presenter)return ii::core::fail(ii::core::ErrorCode::closed,"Page presenter is unavailable");
        auto lease=std::make_shared<CommitLease>(weak_from_this(),std::move(plan));
        std::function<ii::core::Result<bool>()> commit=[lease]()->ii::core::Result<bool>{
            if(!lease->active.load(std::memory_order_acquire))return ii::core::fail(ii::core::ErrorCode::closed,"Page commit callback has expired");
            auto owner=lease->owner.lock();if(!owner)return ii::core::fail(ii::core::ErrorCode::closed,"Page router has retired");
            if(std::this_thread::get_id()!=owner->owner_)return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Page commit requires the owner thread");
            if(lease->used.exchange(true,std::memory_order_acq_rel))return ii::core::fail(ii::core::ErrorCode::busy,"Page commit callback was already used");
            auto result=lease->plan?owner->history_.commit(*lease->plan):ii::core::Result<bool>{true};
            if(result&&*result)lease->committed.store(true,std::memory_order_release);return result;
        };
        ii::core::Result<void> result;
        try {result=presenter->present(entry,std::move(commit));}
        catch(...){result=ii::core::fail(ii::core::ErrorCode::failed,"Page presenter threw",std::current_exception());}
        lease->active.store(false,std::memory_order_release);
        if(!result)return std::unexpected(result.error());
        if(!lease->committed.load(std::memory_order_acquire))return ii::core::fail(ii::core::ErrorCode::failed,"Page presenter did not commit its candidate");
        return true;
    }
    [[nodiscard]] ii::core::Result<bool> present(const NavigationPlan& plan){
        if(!plan.changes())return false;
        return present_entry(presenter_.lock(),*plan.destination(),plan);
    }
    [[nodiscard]] ii::core::Result<bool> route(const NavigationRequest& request,NavigationOperation operation){
        NavigationEntry entry;entry.params=request.params;
        if(request.target){entry.page=request.target;}
        else {
            auto path=normalize_route(request.href);if(!path)return std::unexpected(path.error());
            auto result=resolver_.resolve(*path);if(!result)return std::unexpected(result.error());
            entry.path=std::move(*path);
            entry.page=(*result)->matched?routes_[(*result)->index].page:fallback_;
            if(!entry.page)return false;
            const auto& storage=request.params.storage();
            const auto* supplied=std::get_if<ii::core::Value::Object>(&storage);
            if(!supplied&&request.params.kind()!=ii::core::ValueKind::null)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Path navigation parameters must be an object");
            ii::core::Value::Object merged;
            for(const auto& [key,value]:(*result)->params)merged.emplace(key,ii::core::Value(value));
            if(supplied)for(const auto& [key,value]:*supplied)merged.insert_or_assign(key,value);
            entry.params=ii::core::Value(std::move(merged));
        }
        auto plan=history_.prepare(operation,std::move(entry));if(!plan)return std::unexpected(plan.error());return present(*plan);
    }
    [[nodiscard]] ii::core::Result<bool> navigation(const NavigationRequest& request,NavigationOperation operation){
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        try {auto keep_alive=shared_from_this();Guard guard(active_);return route(request,operation);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page navigation failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<bool> history_operation(NavigationOperation operation){
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        try {auto keep_alive=shared_from_this();Guard guard(active_);auto plan=history_.prepare(operation);if(!plan)return std::unexpected(plan.error());return present(*plan);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page history operation failed",std::current_exception());}
    }
public:
    PageRouter(const PageRouter&)=delete;PageRouter& operator=(const PageRouter&)=delete;
    ~PageRouter()override{active_=true;}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<PageRouter>> create(std::vector<PageRoute> routes,
            std::shared_ptr<const NavigationPage> fallback={}){
        try {auto router=std::shared_ptr<PageRouter>(new PageRouter);auto configured=router->set_routes(std::move(routes));
            if(!configured)return std::unexpected(configured.error());router->fallback_=std::move(fallback);return router;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page router creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_routes(std::vector<PageRoute> routes){
        if(auto checked=ready();!checked)return checked;
        if(routes.size()>65536)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Page route capacity exceeded");
        try {auto keep_alive=shared_from_this();Guard guard(active_);std::vector<std::string> patterns;patterns.reserve(routes.size());for(const auto& route:routes){
                if(!route.pattern.empty()&&!route.page)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"A page route requires a page");patterns.push_back(route.pattern);}
            if(auto configured=resolver_.set_patterns(std::move(patterns));!configured)return configured;
            routes_.swap(routes);routes.clear();return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page route configuration failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_fallback(std::shared_ptr<const NavigationPage> page){
        if(auto checked=ready();!checked)return checked;
        try {auto keep_alive=shared_from_this();Guard guard(active_);fallback_.swap(page);page.reset();return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page fallback configuration failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_presenter(const std::shared_ptr<NavigationPresenter>& presenter){
        if(auto checked=ready();!checked)return checked;presenter_=presenter;return {};
    }
    // Rebuild an already committed entry in a candidate host without changing
    // the current presenter or history. The host can then publish and attach.
    [[nodiscard]] ii::core::Result<bool> synchronize(const std::shared_ptr<NavigationPresenter>& presenter){
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        if(!presenter)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Page presenter is required");
        try {auto keep_alive=shared_from_this();Guard guard(active_);auto snapshot=history_.state();if(!snapshot)return std::unexpected(snapshot.error());
            if(!snapshot->current)return false;return present_entry(presenter,*snapshot->current,std::nullopt);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page synchronization failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<bool> navigate(const NavigationRequest& request)override {
        return navigation(request,request.replace?NavigationOperation::replace:NavigationOperation::push);
    }
    [[nodiscard]] ii::core::Result<bool> set_root(std::string_view path,ii::core::Value params=ii::core::Value(ii::core::Value::Object{})){
        try {return navigation(NavigationRequest{.href=std::string(path),.params=std::move(params)},NavigationOperation::set_root);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Page root request failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<bool> back(){return history_operation(NavigationOperation::pop);}
    [[nodiscard]] ii::core::Result<bool> pop_to_root(){return history_operation(NavigationOperation::pop_to_root);}
    [[nodiscard]] ii::core::Result<PageRouterState> state()const {
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        auto history=history_.state();if(!history)return std::unexpected(history.error());return PageRouterState{std::move(*history),routes_.size(),!presenter_.expired()};
    }
    [[nodiscard]] ii::core::Result<std::vector<NavigationEntry>> entries()const {
        if(auto checked=ready();!checked)return std::unexpected(checked.error());return history_.entries();
    }
};
} // namespace lvui
