#pragma once
#include <LVUI/NativeActivation.h>
#include <any>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>

namespace lvui::native {
struct BackendRequest final {std::string_view type,binding;BackendActivation activation{};};
template<class T>concept OwnedBackend = std::is_object_v<T> && !std::is_array_v<T>;
template<class T,class F>concept BackendFactory = OwnedBackend<T> && (requires(F& factory){
    { std::invoke(factory) } -> std::same_as<T>;
} || requires(F& factory,const BackendRequest& request){
    { std::invoke(factory,request) } -> std::same_as<T>;
});
// Immutable, activation-local constructor inputs. This registry exposes no
// backend members and never shares a constructed backend between activations.
class BackendFactories final {
    using Entries=std::map<std::type_index,std::any>;
public:
    static constexpr std::size_t maximum_factories=4096;
    BackendFactories()=default;
    template<class T,class Factory>
        requires BackendFactory<T,Factory>
    [[nodiscard]] BackendFactories with(Factory factory)const{
        auto next=entries_?std::make_shared<Entries>(*entries_):std::make_shared<Entries>();
        const auto key=std::type_index(typeid(std::type_identity<T>));
        if(!next->contains(key)&&next->size()>=maximum_factories)throw std::length_error("Native constructor policy exceeds 4096 types");
        // The erased function is copyable even when the supplied factory owns
        // move-only dependencies. The returned T still has guaranteed elision.
        auto owned=std::make_shared<Factory>(std::move(factory));
        next->insert_or_assign(key,std::function<T(const BackendRequest&)>([owned](const BackendRequest& request){
            if constexpr(requires{{std::invoke(*owned,request)}->std::same_as<T>;})return std::invoke(*owned,request);
            else return std::invoke(*owned);
        }));
        return BackendFactories(std::move(next),activation_);
    }
    template<class F>requires std::same_as<std::invoke_result_t<F&>,BackendActivation>
    [[nodiscard]] BackendFactories with_activation(F factory)const{
        auto owned=std::make_shared<F>(std::move(factory));
        return BackendFactories(entries_,[owned]{return std::invoke(*owned);});
    }
    [[nodiscard]] BackendActivation activate()const{return activation_?activation_():BackendActivation{};}
    template<OwnedBackend T>[[nodiscard]] T create(std::string_view name={},std::string_view binding={},BackendActivation activation={})const{
        if(entries_)if(auto found=entries_->find(std::type_index(typeid(std::type_identity<T>)));found!=entries_->end())
            return std::any_cast<const std::function<T(const BackendRequest&)>&>(found->second)({name,binding,std::move(activation)});
        if constexpr(std::default_initializable<T>)return T();
        else throw std::logic_error("Native constructor factory required"+(name.empty()?std::string{}:": "+std::string(name)));
    }
    [[nodiscard]] std::size_t size()const noexcept{return entries_?entries_->size():0;}
private:
    explicit BackendFactories(std::shared_ptr<const Entries> entries,std::function<BackendActivation()> activation={}):entries_(std::move(entries)),activation_(std::move(activation)){}
    std::shared_ptr<const Entries> entries_;
    std::function<BackendActivation()> activation_;
};
} // namespace lvui::native
