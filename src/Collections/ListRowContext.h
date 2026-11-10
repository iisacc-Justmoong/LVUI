#pragma once

#include <LVUI/List.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace lvui::native {
// A command keeps only a weak reference to its owning row generation.
// Retaining it cannot keep a list alive or redirect it to a successor row.
class ListRowAction final {
public:
    [[nodiscard]] ii::core::Result<void> request()const{
        try{return handler_();}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List row command failed",std::current_exception());}
    }
    void operator()()const{
        if(auto result=request();!result){
            if(result.error().cause)std::rethrow_exception(result.error().cause);
            throw std::runtime_error(result.error().message);
        }
    }
private:
    friend struct ListRowContextPort;
    explicit ListRowAction(std::function<ii::core::Result<void>()> handler):handler_(std::move(handler)){}
    std::function<ii::core::Result<void>()> handler_;
};
struct ListRowSnapshot final {
    ListEntry entry;
    std::size_t source_index=0;
    bool selected=false,enabled=true;
    double height=20;
    std::shared_ptr<const ListRowAction> trigger;
    friend bool operator==(const ListRowSnapshot&,const ListRowSnapshot&)=default;
};

// A row generation owns this read-only observable until it leaves the viewport.
class ListRowContext final {
public:
    [[nodiscard]] const ListRowSnapshot& get()const{check();return value_->get();}
    [[nodiscard]] std::uint64_t revision()const{check();return value_->revision();}
    [[nodiscard]] ii::core::ObservableSubscription observe(
            std::function<void(const ListRowSnapshot&,std::uint64_t)> callback,bool initial=false)const{
        check();return value_->observe(std::move(callback),initial);
    }
    [[nodiscard]] ii::core::ObservableRead read_dependency()const{check();return value_->read_dependency();}
    [[nodiscard]] ii::core::Result<void> trigger()const{
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List row context requires its owner thread");
        if(!value_)return ii::core::fail(ii::core::ErrorCode::closed,"List row context is closed");
        auto action=value_->get().trigger;
        return action->request();
    }
private:
    friend struct ListRowContextPort;
    explicit ListRowContext(ListRowSnapshot initial):value_(std::in_place,std::move(initial)){}
    void check()const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("List row context requires its owner thread");
        if(!value_)throw std::logic_error("List row context is closed");
    }
    void publish(ListRowSnapshot next){check();value_->set(std::move(next));}
    void close()noexcept{value_.reset();}
    const std::thread::id owner_=std::this_thread::get_id();
    std::optional<ii::core::Observable<ListRowSnapshot>> value_;
};
struct ListRowContextPort final {
    [[nodiscard]] static std::shared_ptr<const ListRowAction> make_action(std::function<ii::core::Result<void>()> callback){return std::shared_ptr<const ListRowAction>(new ListRowAction(std::move(callback)));}
    [[nodiscard]] static std::shared_ptr<ListRowContext> make(ListRowSnapshot value){return std::shared_ptr<ListRowContext>(new ListRowContext(std::move(value)));}
    static void publish(ListRowContext& context,ListRowSnapshot value){context.publish(std::move(value));}
    static void close(ListRowContext& context)noexcept{context.close();}
};
} // namespace lvui::native
