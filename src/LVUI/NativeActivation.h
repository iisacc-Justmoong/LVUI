#pragma once
#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <iiSDKCore/Core/Result/Result.h>

namespace lvui::native {
// Optional typed activation resource. Native-only consumers need no worker
// runtime; an application installs the resource through BackendFactories.
class BackendActivation final {
    struct State final {
        std::any context;
        std::function<void(std::weak_ptr<void>,std::function<void()>)> attach;
        std::function<void(std::shared_ptr<void>)> retire;
        std::weak_ptr<void> owner_scope;
        std::thread::id owner=std::this_thread::get_id();
        bool attached=false,closed=false;
    };
public:
    BackendActivation()=default;
    template<class T>static BackendActivation from(std::shared_ptr<T> context){
        if(!context)throw std::invalid_argument("Activation context cannot be null");
        auto state=std::make_shared<State>();state->context=context;
        state->attach=[context](auto owner,auto changed){context->attach(std::move(owner),std::move(changed));};
        state->retire=[context](auto owner){context->retire(std::move(owner));};
        return BackendActivation(std::move(state));
    }
    template<class T>std::shared_ptr<T> get()const{
        check();if(!state_)return {};
        const auto* value=std::any_cast<std::shared_ptr<T>>(&state_->context);
        if(!value)throw std::logic_error("Activation context has a different C++ type");return *value;
    }
    void attach(const std::shared_ptr<void>& owner,std::function<void()> changed={})const{
        check();if(!state_)return;
        if(!owner||state_->attached||state_->closed)throw std::logic_error("Activation cannot attach its owner");
        state_->attach(owner,std::move(changed));state_->owner_scope=owner;state_->attached=true;
    }
    void retire(std::shared_ptr<void> owner)const{
        check();if(!state_||state_->closed)return;
        if(state_->attached&&state_->owner_scope.lock()!=owner)throw std::logic_error("Activation retirement must retain its original owner");
        state_->closed=true;
        if(state_->attached)state_->retire(std::move(owner));
    }
private:
    explicit BackendActivation(std::shared_ptr<State> state):state_(std::move(state)){}
    void check()const{if(state_&&state_->owner!=std::this_thread::get_id())throw std::logic_error("Activation requires its owner thread");}
    std::shared_ptr<State> state_;
};
template<class Scope,class Builder>
ii::core::Result<void> attach_backend_activation(const std::shared_ptr<Scope>& scope,Builder& widgets){
    if(auto retiring=widgets.on_retire([weak=std::weak_ptr(scope)]{if(auto owner=weak.lock())owner->backend_activation.retire(owner);});!retiring)return retiring;
    scope->backend_activation.attach(scope,[weak=std::weak_ptr(scope)]{if(auto owner=weak.lock())owner->publish_native_changes();});
    return {};
}
} // namespace lvui::native
