#pragma once
#include <LVUI/Navigation.h>
#include <LVUI/Route.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace lvui {
struct NavigationEntry final {
    std::string path;
    ii::core::Value params{ii::core::Value::Object{}};
    std::shared_ptr<const NavigationPage> page;
    bool enabled=true;
    friend bool operator==(const NavigationEntry&,const NavigationEntry&)=default;
};
enum class NavigationOperation {push,replace,set_root,pop,pop_to_root,clear};
struct NavigationHistoryState final {
    std::uint64_t revision=0;
    std::size_t depth=0,retained_bytes=0;
    std::optional<NavigationEntry> current;
};

class NavigationHistory;
class NavigationPlan final {
    friend class NavigationHistory;
    std::weak_ptr<const void> identity_;
    std::uint64_t revision_=0;
    NavigationOperation operation_=NavigationOperation::push;
    std::shared_ptr<const NavigationEntry> destination_;
    std::size_t entry_cost_=0,result_bytes_=0;
    bool changes_=false;
    NavigationPlan()=default;
public:
    [[nodiscard]] NavigationOperation operation()const noexcept{return operation_;}
    [[nodiscard]] bool changes()const noexcept{return changes_;}
    [[nodiscard]] const NavigationEntry* destination()const noexcept{return destination_.get();}
    [[nodiscard]] std::uint64_t base_revision()const noexcept{return revision_;}
};

// A plan contains an immutable candidate, but does not change history. A host
// can build the candidate view and commit only after that build succeeds.
class NavigationHistory final {
    static constexpr std::size_t max_entries=65536,max_entry_bytes=2*1024*1024,max_total_bytes=16*1024*1024;
    struct Stored final {NavigationEntry entry;std::size_t cost=0;};
    static_assert(std::is_nothrow_move_constructible_v<Stored>&&std::is_nothrow_move_assignable_v<Stored>);
    std::thread::id owner_=std::this_thread::get_id();
    std::shared_ptr<const int> identity_=std::make_shared<const int>(0);
    std::vector<Stored> entries_;
    std::uint64_t revision_=0;
    std::size_t retained_bytes_=0;
    bool active_=false;
    struct Guard final {bool& active;explicit Guard(bool& v):active(v){active=true;}~Guard(){active=false;}};
    [[nodiscard]] ii::core::Result<void> ready()const {
        if(std::this_thread::get_id()!=owner_)return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Navigation history requires its owner thread");
        if(active_)return ii::core::fail(ii::core::ErrorCode::busy,"Navigation history is changing");return {};
    }
    [[nodiscard]] static ii::core::Result<std::size_t> prepare_entry(NavigationEntry& entry){
        if(!entry.page||!entry.path.empty()){
            auto path=normalize_route(entry.path);if(!path)return std::unexpected(path.error());entry.path=std::move(*path);
        }
        if(entry.params.kind()==ii::core::ValueKind::null)entry.params=ii::core::Value(ii::core::Value::Object{});
        const auto stats=navigation_parameter_stats(entry.params);
        if(!stats)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid navigation parameters");
        const auto cost=sizeof(NavigationEntry)+entry.path.size()+stats->bytes+stats->nodes*sizeof(ii::core::Value);
        if(cost>max_entry_bytes)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Navigation entry budget exceeded");
        return cost;
    }
public:
    NavigationHistory()=default;
    NavigationHistory(const NavigationHistory&)=delete;NavigationHistory& operator=(const NavigationHistory&)=delete;
    NavigationHistory(NavigationHistory&&)=delete;NavigationHistory& operator=(NavigationHistory&&)=delete;
    ~NavigationHistory(){active_=true;entries_.clear();}
    [[nodiscard]] ii::core::Result<NavigationPlan> prepare(NavigationOperation operation,std::optional<NavigationEntry> input=std::nullopt){
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        if(unsigned(operation)>unsigned(NavigationOperation::clear))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Unknown navigation operation");
        const bool needs_entry=operation==NavigationOperation::push||operation==NavigationOperation::replace||operation==NavigationOperation::set_root;
        if(needs_entry!=input.has_value())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Navigation operation has invalid arguments");
        try {
            NavigationPlan plan;plan.identity_=identity_;plan.revision_=revision_;plan.operation_=operation;plan.result_bytes_=retained_bytes_;
            if(needs_entry){
                auto cost=prepare_entry(*input);if(!cost)return std::unexpected(cost.error());plan.entry_cost_=*cost;
                if(operation==NavigationOperation::push){
                    if(entries_.size()==max_entries||*cost>max_total_bytes-retained_bytes_)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Navigation history budget exceeded");
                    plan.result_bytes_=retained_bytes_+*cost;plan.changes_=true;
                }else if(operation==NavigationOperation::replace){
                    const auto retained=entries_.empty()?0:retained_bytes_-entries_.back().cost;
                    if(*cost>max_total_bytes-retained)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Navigation history budget exceeded");
                    plan.result_bytes_=retained+*cost;plan.changes_=entries_.empty()||entries_.back().entry!=*input;
                }else {plan.result_bytes_=*cost;plan.changes_=entries_.size()!=1||entries_.front().entry!=*input;}
                plan.destination_=std::make_shared<const NavigationEntry>(std::move(*input));
            }else if(operation==NavigationOperation::pop||operation==NavigationOperation::pop_to_root){
                if(!entries_.empty()){
                    const auto index=operation==NavigationOperation::pop&&entries_.size()>1?entries_.size()-2:0;
                    plan.destination_=std::make_shared<const NavigationEntry>(entries_[index].entry);
                    plan.changes_=entries_.size()>1;
                    if(plan.changes_)plan.result_bytes_=operation==NavigationOperation::pop?retained_bytes_-entries_.back().cost:entries_.front().cost;
                }
            }else {plan.changes_=!entries_.empty();plan.result_bytes_=0;}
            if(plan.changes_&&revision_==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Navigation revision exhausted");
            return plan;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Navigation plan preparation failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<bool> commit(const NavigationPlan& plan){
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        const auto identity=plan.identity_.lock();
        if(!identity||identity.get()!=identity_.get())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Navigation plan belongs to another history");
        if(plan.revision_!=revision_)return ii::core::fail(ii::core::ErrorCode::busy,"Navigation plan is stale");
        if(!plan.changes_)return false;
        Guard guard(active_);
        try {
            switch(plan.operation_){
            case NavigationOperation::push:{Stored next{*plan.destination_,plan.entry_cost_};entries_.push_back(std::move(next));break;}
            case NavigationOperation::replace:{Stored next{*plan.destination_,plan.entry_cost_};
                if(entries_.empty())entries_.push_back(std::move(next));else std::swap(entries_.back(),next);break;}
            case NavigationOperation::set_root:{std::vector<Stored> next;next.push_back({*plan.destination_,plan.entry_cost_});entries_.swap(next);break;}
            case NavigationOperation::pop:entries_.pop_back();break;
            case NavigationOperation::pop_to_root:entries_.erase(entries_.begin()+1,entries_.end());break;
            case NavigationOperation::clear:entries_.clear();break;
            }
            retained_bytes_=plan.result_bytes_;++revision_;return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Navigation commit failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<NavigationHistoryState> state()const {
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        try {NavigationHistoryState result{revision_,entries_.size(),retained_bytes_,std::nullopt};if(!entries_.empty())result.current=entries_.back().entry;return result;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Navigation state snapshot failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::vector<NavigationEntry>> entries()const {
        if(auto checked=ready();!checked)return std::unexpected(checked.error());
        try {std::vector<NavigationEntry> result;result.reserve(entries_.size());for(const auto& entry:entries_)result.push_back(entry.entry);return result;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Navigation history snapshot failed",std::current_exception());}
    }
};
} // namespace lvui
