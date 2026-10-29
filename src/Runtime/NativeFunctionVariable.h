#pragma once
#include <LVUI/NativeValues.h>

namespace lvui::native {
// One owner-thread slot per declaration. Only the graph owns the slot; saved
// write handles cannot retain its native captures after activation retirement.
template<class Signature>class FunctionVariable final : public std::enable_shared_from_this<FunctionVariable<Signature>> {
    using Value=CallbackValue<Signature>;
    using Action=typename Value::Action;
    using Result=typename detail::CallbackInvocation<Signature>::Result;
    struct Slot final {
        explicit Slot(std::shared_ptr<Action> value):current(std::move(value)){}
        ii::core::Observable<std::shared_ptr<Action>> current;
    };
public:
    FunctionVariable(const FunctionVariable&)=delete;
    FunctionVariable& operator=(const FunctionVariable&)=delete;
    [[nodiscard]] std::shared_ptr<Value> value()const{return value_;}

    template<class Scope>static std::shared_ptr<FunctionVariable> from_value(
            const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph,
            const std::shared_ptr<Value>& initial){
        validate_owner(scope,graph);
        if(!initial)throw std::invalid_argument("A function variable requires an initial value");
        auto source=initial;
        return from_action(scope,graph,source->capture_current());
    }
    template<class Scope,class Factory,class Invoke>static std::shared_ptr<FunctionVariable> from_native(
            const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph,
            Factory factory,Invoke invoke_handler){
        validate_owner(scope,graph);
        auto selected=select_native(factory);
        auto invoke=std::make_shared<Invoke>(std::move(invoke_handler));
        auto action=std::make_shared<Action>(scoped_function<Result>(scope,graph,
            [selected,invoke](const auto& owner,auto&&... arguments)->Result{
                detail::require_callable(*selected);
                return detail::function_result<Result>([&]()->decltype(auto){
                    return std::invoke(*invoke,owner,*selected,std::forward<decltype(arguments)>(arguments)...);
                });
            }));
        return from_action(scope,graph,std::move(action));
    }

    [[nodiscard]] ii::core::Result<void> set(const std::shared_ptr<Value>& replacement){
        if(thread_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"A function variable requires its owner thread");
        auto keep_alive=this->shared_from_this();
        auto slot=access();if(!slot)return std::unexpected(slot.error());
        auto source=replacement;
        if(!source)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"A function variable cannot contain an empty value");
        if(source->state_->thread!=thread_)return ii::core::fail(ii::core::ErrorCode::wrong_executor,"A function value requires its owner thread");
        try{
            if(!source->available())return ii::core::fail(ii::core::ErrorCode::closed,"The replacement function owner retired");
            if(source==value_)return {};
            return commit(*slot,source->capture_current());
        }
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Function selection failed",std::current_exception());}
    }
    template<class Factory>[[nodiscard]] ii::core::Result<void> replace_native(Factory factory){
        if(thread_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"A function variable requires its owner thread");
        auto keep_alive=this->shared_from_this();
        auto slot=access();if(!slot)return std::unexpected(slot.error());
        try{
            auto selected=select_native(factory);
            auto action=std::make_shared<Action>([selected](auto&&... arguments)->Result{
                return invoke_callback<Signature>(*selected,std::forward<decltype(arguments)>(arguments)...);
            });
            if(!value_->available())return ii::core::fail(ii::core::ErrorCode::closed,"The function variable owner retired during replacement");
            return commit(*slot,native_gate_(std::move(action)));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Native function replacement failed",std::current_exception());}
    }
private:
    FunctionVariable()=default;
    template<class Scope>static void validate_owner(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph){
        if(!scope||!graph||!graph->available())throw std::logic_error("A function variable requires an active owner domain");
        if constexpr(requires{scope->active();})if(!scope->active())throw std::logic_error("The function variable scope retired");
    }
    template<class Factory>static auto select_native(Factory& factory){
        using Returned=std::invoke_result_t<Factory&>;
        using Handler=std::remove_reference_t<Returned>;
        static_assert(std::is_object_v<Handler>||(std::is_lvalue_reference_v<Returned>&&std::is_function_v<Handler>),
                      "A native function factory must return an invocable object or function reference");
        auto selected=[&]{
            if constexpr(std::is_lvalue_reference_v<Returned>)return std::addressof(std::invoke(factory));
            else return std::shared_ptr<Handler>(new Handler(std::invoke(factory)));
        }();
        detail::require_callable(*selected);return selected;
    }
    template<class Scope>static std::shared_ptr<FunctionVariable> from_action(
            const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph,std::shared_ptr<Action> initial){
        auto variable=std::shared_ptr<FunctionVariable>(new FunctionVariable);
        auto slot=std::make_shared<Slot>(std::move(initial));
        variable->slot_=slot;variable->graph_=graph;
        variable->value_=make_callback_value<Signature>(scope,graph,[scope,slot](ReadContext& context){
            (void)scope;return context.read(slot->current);
        },[](const auto&,auto& action,auto&&... arguments)->Result{
            return std::invoke(*action,std::forward<decltype(arguments)>(arguments)...);
        });
        // Selecting a variable copies the native version, not a chain of
        // mutable-slot forwarding wrappers. The version carries its actual
        // receiver's gate; the source variable storage is not the receiver.
        variable->value_->state_->snapshot=[weak_slot=std::weak_ptr(slot)]{
            auto current=weak_slot.lock();if(!current)throw std::logic_error("The selected function variable retired");
            return current->current.get();
        };
        variable->native_gate_=[weak_scope=std::weak_ptr(scope),weak_graph=std::weak_ptr(graph)](std::shared_ptr<Action> action){
            auto graph=weak_graph.lock();if(!graph||!graph->available())throw std::logic_error("The native function variable owner retired");
            auto owner=weak_scope.lock();if(!owner)throw std::logic_error("The native function variable receiver retired");
            return std::make_shared<Action>(scoped_function<Result>(owner,graph,[action](const auto&,auto&&... arguments)->Result{
                return std::invoke(*action,std::forward<decltype(arguments)>(arguments)...);
            }));
        };
        return variable;
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<Slot>> access()const{
        if(thread_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"A function variable requires its owner thread");
        auto graph=graph_.lock();if(!graph||!graph->available()||!value_)
            return ii::core::fail(ii::core::ErrorCode::closed,"The function variable owner retired");
        try{if(!value_->available())return ii::core::fail(ii::core::ErrorCode::closed,"The function variable receiver retired");}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Function variable admission failed",std::current_exception());}
        auto slot=slot_.lock();if(!slot)return ii::core::fail(ii::core::ErrorCode::closed,"The function variable slot retired");
        return slot;
    }
    ii::core::Result<void> commit(const std::shared_ptr<Slot>& slot,std::shared_ptr<Action> selected){
        // A user C++ snapshot/factory may have retired the target meanwhile.
        if(!value_->available())return ii::core::fail(ii::core::ErrorCode::closed,"The function variable owner retired during replacement");
        (void)slot->current.set(std::move(selected));return {};
    }
    const std::thread::id thread_=std::this_thread::get_id();
    std::weak_ptr<Slot> slot_;
    std::weak_ptr<Bindings> graph_;
    std::shared_ptr<Value> value_;
    std::function<std::shared_ptr<Action>(std::shared_ptr<Action>)> native_gate_;
};
} // namespace lvui::native
