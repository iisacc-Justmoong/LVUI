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
class QueryConnections;
class AppStrings;
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
        return BackendFactories(std::move(next),activation_,queries_,strings_);
    }
    template<class F>requires std::same_as<std::invoke_result_t<F&>,BackendActivation>
    [[nodiscard]] BackendFactories with_activation(F factory)const{
        auto owned=std::make_shared<F>(std::move(factory));
        return BackendFactories(entries_,[owned]{return std::invoke(*owned);},queries_,strings_);
    }
    [[nodiscard]] BackendFactories with_query_connections(std::shared_ptr<QueryConnections> connections)const{
        if(!connections)throw std::invalid_argument("Query connections cannot be null");
        return BackendFactories(entries_,activation_,std::move(connections),strings_);
    }
    [[nodiscard]] std::shared_ptr<QueryConnections> query_connections()const noexcept{return queries_;}
    [[nodiscard]] BackendFactories with_app_strings(std::shared_ptr<AppStrings> strings)const{
        if(!strings)throw std::invalid_argument("App string storage cannot be null");
        return BackendFactories(entries_,activation_,queries_,std::move(strings));
    }
    [[nodiscard]] std::shared_ptr<AppStrings> app_strings()const noexcept{return strings_;}
    [[nodiscard]] BackendActivation activate()const{return activation_?activation_():BackendActivation{};}
    template<OwnedBackend T>[[nodiscard]] T create(std::string_view name={},std::string_view binding={},BackendActivation activation={})const{
        if(entries_)if(auto found=entries_->find(std::type_index(typeid(std::type_identity<T>)));found!=entries_->end())
            return std::any_cast<const std::function<T(const BackendRequest&)>&>(found->second)({name,binding,std::move(activation)});
        if constexpr(std::default_initializable<T>)return T();
        else throw std::logic_error("Native constructor factory required"+(name.empty()?std::string{}:": "+std::string(name)));
    }
    [[nodiscard]] std::size_t size()const noexcept{return entries_?entries_->size():0;}
private:
    explicit BackendFactories(std::shared_ptr<const Entries> entries,std::function<BackendActivation()> activation={},std::shared_ptr<QueryConnections> queries={},std::shared_ptr<AppStrings> strings={})
        :entries_(std::move(entries)),activation_(std::move(activation)),queries_(std::move(queries)),strings_(std::move(strings)){}
    std::shared_ptr<const Entries> entries_;
    std::function<BackendActivation()> activation_;
    std::shared_ptr<QueryConnections> queries_;
    std::shared_ptr<AppStrings> strings_;
};
} // namespace lvui::native
