#pragma once
#include "NativeFunctionVariable.h"
#include "NativeMethods.h"

namespace lvui::native::inferred {
template<class T>struct NormalType {using type=std::conditional_t<String<const std::remove_reference_t<T>&>,std::string,
    std::conditional_t<std::same_as<std::remove_cvref_t<T>,bool>,bool,
    std::conditional_t<std::integral<std::remove_cvref_t<T>>,std::int64_t,std::remove_cvref_t<T>>>>;};
template<>struct NormalType<void>{using type=void;};
template<class T>using Normal=typename NormalType<std::remove_cvref_t<T>>::type;
template<class T,class=void>struct Signature {};
template<class Result,class... Args>struct Signature<Result(Args...),void>{using type=Normal<Result>(Normal<Args>...);};
template<class Result,class... Args>struct Signature<Result(*)(Args...),void>:Signature<Result(Args...)>{};
template<class Result,class... Args>struct Signature<Result(*)(Args...)noexcept,void>:Signature<Result(Args...)>{};
template<class Result,class... Args>struct Signature<std::function<Result(Args...)>,void>:Signature<Result(Args...)>{};
#define LVUI_INFER_MEMBER(CVREF,EXCEPT) \
template<class C,class Result,class... Args>struct Signature<Result(C::*)(Args...) CVREF EXCEPT,void>:Signature<Result(Args...)>{};
LVUI_INFER_MEMBER(,)
LVUI_INFER_MEMBER(const,)
LVUI_INFER_MEMBER(volatile,)
LVUI_INFER_MEMBER(const volatile,)
LVUI_INFER_MEMBER(&,)
LVUI_INFER_MEMBER(const &,)
LVUI_INFER_MEMBER(volatile &,)
LVUI_INFER_MEMBER(const volatile &,)
LVUI_INFER_MEMBER(&&,)
LVUI_INFER_MEMBER(const &&,)
LVUI_INFER_MEMBER(volatile &&,)
LVUI_INFER_MEMBER(const volatile &&,)
LVUI_INFER_MEMBER(,noexcept)
LVUI_INFER_MEMBER(const,noexcept)
LVUI_INFER_MEMBER(volatile,noexcept)
LVUI_INFER_MEMBER(const volatile,noexcept)
LVUI_INFER_MEMBER(&,noexcept)
LVUI_INFER_MEMBER(const &,noexcept)
LVUI_INFER_MEMBER(volatile &,noexcept)
LVUI_INFER_MEMBER(const volatile &,noexcept)
LVUI_INFER_MEMBER(&&,noexcept)
LVUI_INFER_MEMBER(const &&,noexcept)
LVUI_INFER_MEMBER(volatile &&,noexcept)
LVUI_INFER_MEMBER(const volatile &&,noexcept)
#undef LVUI_INFER_MEMBER
template<class T>struct Signature<T,std::void_t<decltype(&T::operator())>>:Signature<decltype(&T::operator())>{};
template<class T>struct Target {using type=std::remove_cvref_t<T>;};
template<class T>struct Target<std::shared_ptr<T>>:Target<T>{};
template<class T>struct Target<std::unique_ptr<T>>:Target<T>{};
template<class T>struct Target<std::reference_wrapper<T>>:Target<T>{};
template<class T>requires(!std::is_function_v<T>)struct Target<T*>:Target<T>{};
template<class T>using TargetType=typename Target<std::remove_cvref_t<T>>::type;
template<class T>concept Callable=requires{typename Signature<TargetType<T>>::type;};
template<class T>struct CallbackPointer:std::false_type{};
template<class S>struct CallbackPointer<std::shared_ptr<CallbackValue<S>>>:std::true_type{using signature=S;};
struct Owners {
private:
    friend class lvui::native::Methods;
    std::vector<std::shared_ptr<const void>> owners_;
public:
    const auto& retained()const{return owners_;}
    bool same_owners(const Owners& other)const{
        if(owners_.size()!=other.owners_.size())return false;
        for(std::size_t i=0;i<owners_.size();++i)
            if(owners_[i].owner_before(other.owners_[i])||other.owners_[i].owner_before(owners_[i]))return false;
        return true;
    }
};
template<class T>struct Plain final:Owners {
    static constexpr bool function=false;
    using value_type=T;
    T value;
    explicit Plain(T selected):value(std::move(selected)){}
    bool operator==(const Plain& other)const requires std::equality_comparable<T>{return value==other.value&&same_owners(other);}
};
struct Direct final {template<class Scope,class Action>decltype(auto) operator()(const Scope&,Action action)const{return action();}};
template<class S,class Handler,class Publication=Direct>struct Function final:Owners {
    static constexpr bool function=true;
    using signature=S;
    Handler selected;
    Publication publication;
    explicit Function(Handler value,Publication publish={}):selected(std::move(value)),publication(std::move(publish)){}
    decltype(auto) handler(){return (selected);}
    template<class Scope,class... Args>auto invoke(const Scope& owner,Args&&... args)->typename detail::CallbackInvocation<S>::Result{
        if constexpr(std::same_as<typename detail::CallbackInvocation<S>::Result,void>)
            return publication(owner,[&]{return invoke_callback<S>(handler(),std::forward<Args>(args)...);});
        else return invoke_callback<S>(handler(),std::forward<Args>(args)...);
    }
};
template<class S>struct Alias final:Owners {
    static constexpr bool function=true;
    using signature=S;
    std::shared_ptr<typename CallbackValue<S>::Action> selected;
    explicit Alias(const std::shared_ptr<CallbackValue<S>>& value):selected(value->select()){}
    decltype(auto) handler(){return (*selected);}
    template<class Scope,class... Args>auto invoke(const Scope&,Args&&... args)->typename detail::CallbackInvocation<S>::Result{return invoke_callback<S>(handler(),std::forward<Args>(args)...);}
};
template<class Publication=Direct>struct Marker final {
    Publication publication{};
    template<class T>static constexpr bool is_alias=CallbackPointer<std::remove_cvref_t<T>>::value;
    template<class T>static constexpr bool is_callable=Callable<T>;
    template<class T>auto plain(T value)const{return Plain<T>(std::move(value));}
    template<class T>auto alias(const T& value)const{
        if(!value)throw std::invalid_argument("An inferred function alias cannot be empty");
        return Alias<typename CallbackPointer<std::remove_cvref_t<T>>::signature>(value);
    }
    template<class Original,class Handler>auto callable(Handler value)const{
        return Function<typename Signature<TargetType<Original>>::type,Handler,Publication>(std::move(value),publication);
    }
    template<class Pointer,class Handler>auto method(Handler value)const{
        return Function<typename Signature<Pointer>::type,Handler,Publication>(std::move(value),publication);
    }
};
enum class Mode {readonly, fixed, variable};
template<class Definition,Mode Storage,bool Function=Definition::function>class Slot;
template<class Definition,Mode Storage>class Slot<Definition,Storage,false> final:public std::enable_shared_from_this<Slot<Definition,Storage,false>> {
    using Self=Slot;
    std::optional<ii::core::Observable<Definition>> owned_;
    std::shared_ptr<ReadOnlyValue<Definition>> derived_;
    std::weak_ptr<Bindings> graph_;
    explicit Slot(const std::shared_ptr<Bindings>& graph):graph_(graph){}
    const Definition& definition()const{check();if constexpr(Storage==Mode::readonly)return derived_->get();else return owned_->get();}
    void check()const{auto graph=graph_.lock();if(!graph||!graph->available())throw std::logic_error("Inferred value owner retired or wrong execution domain");}
public:
    using value_type=typename Definition::value_type;
    template<class Scope,class Initial,class Formula>static auto make(const std::shared_ptr<Scope>&,const std::shared_ptr<Bindings>& graph,Initial initial,Formula formula){
        auto slot=std::shared_ptr<Self>(new Self(graph));
        if constexpr(Storage==Mode::readonly){
            auto source=graph->derive(std::move(formula));
            if(!source){if(source.error().cause)std::rethrow_exception(source.error().cause);throw std::runtime_error(source.error().message);}
            slot->derived_=*source;
        }else slot->owned_.emplace(initial());
        return slot;
    }
    const value_type& get()const{return definition().value;}
    std::uint64_t revision()const{check();if constexpr(Storage==Mode::readonly)return derived_->revision();else return owned_->revision();}
    ii::core::ObservableSubscription observe(std::function<void(const value_type&,std::uint64_t)> changed,bool initial=false)const{
        check();auto forward=[changed=std::move(changed)](const Definition& next,std::uint64_t revision){changed(next.value,revision);};
        if constexpr(Storage==Mode::readonly)return derived_->observe(std::move(forward),initial);else return owned_->observe(std::move(forward),initial);
    }
    ii::core::ObservableRead read_dependency()const{
        check();auto self=this->shared_from_this();auto owners=std::make_shared<std::vector<std::shared_ptr<const void>>>(definition().retained());
        return {self.get(),revision(),[self](std::function<void()> changed){return self->observe([changed=std::move(changed)](const auto&,std::uint64_t){changed();});},[self]{return self->revision();},std::shared_ptr<const void>(self,nullptr),owners};
    }
    ii::core::Result<void> set(value_type value)requires(Storage==Mode::variable){
        try{check();owned_->set(Definition(std::move(value)));return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Inferred value assignment failed",std::current_exception());}
    }
};
template<class Definition,Mode Storage>class Slot<Definition,Storage,true> final:public std::enable_shared_from_this<Slot<Definition,Storage,true>> {
    using Self=Slot;
    using S=typename Definition::signature;
    std::shared_ptr<CallbackValue<S>> value_;
    std::shared_ptr<FunctionVariable<S>> variable_;
public:
    using value_type=std::shared_ptr<CallbackValue<S>>;
    template<class Scope,class Initial,class Formula>static auto make(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph,Initial initial,Formula formula){
        auto slot=std::make_shared<Self>();
        auto invoke=[](const auto& owner,auto& selected,auto&&... args)->typename detail::CallbackInvocation<S>::Result{
            return selected.invoke(owner,std::forward<decltype(args)>(args)...);
        };
        if constexpr(Storage==Mode::variable){slot->variable_=FunctionVariable<S>::from_native(scope,graph,std::move(initial),std::move(invoke));slot->value_=slot->variable_->value();}
        else if constexpr(Storage==Mode::fixed)slot->value_=make_callback_value<S>(scope,graph,[initial=std::move(initial)](ReadContext&)mutable{return initial();},std::move(invoke));
        else slot->value_=make_callback_value<S>(scope,graph,std::move(formula),std::move(invoke));
        return slot;
    }
    const value_type& get()const{return value_;}
    std::uint64_t revision()const{return value_->revision();}
    ii::core::ObservableSubscription observe(std::function<void(const value_type&,std::uint64_t)> changed,bool initial=false)const{
        auto value=value_;return value->observe([value,changed=std::move(changed)](std::uint64_t revision){changed(value,revision);},initial);
    }
    ii::core::ObservableRead read_dependency()const{
        auto self=this->shared_from_this();return {self.get(),revision(),[self](std::function<void()> changed){return self->observe([changed=std::move(changed)](const auto&,std::uint64_t){changed();});},[self]{return self->revision();},std::shared_ptr<const void>(self,nullptr),value_};
    }
    ii::core::Result<void> set(const value_type& value)requires(Storage==Mode::variable){return variable_->set(value);}
    template<class Factory>ii::core::Result<void> replace_native(Factory factory)requires(Storage==Mode::variable){return variable_->replace_native(std::move(factory));}
};
template<Mode Storage,class Scope,class Initial,class Formula>auto make(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& graph,Initial initial,Formula formula){
    using Definition=std::remove_cvref_t<std::invoke_result_t<Initial&>>;
    return Slot<Definition,Storage>::make(scope,graph,std::move(initial),std::move(formula));
}
template<class Expected,class Actual>decltype(auto) expression_argument(Actual&& value){
    if constexpr(std::same_as<Expected,std::int64_t>&&Numeric<std::remove_cvref_t<Actual>>)return integer(value);
    else return std::forward<Actual>(value);
}
template<class S>struct ExpressionCall;
template<class Result,class... Expected>struct ExpressionCall<Result(Expected...)> final {
    template<class Invoke,class... Actual>static Result call(Invoke invoke,Actual&&... arguments){
        static_assert(sizeof...(Actual)==sizeof...(Expected),"Inferred function argument count does not match its native signature");
        if constexpr(sizeof...(Actual)==sizeof...(Expected))return invoke(expression_argument<Expected>(std::forward<Actual>(arguments))...);
    }
};
template<class Source,class... Args>decltype(auto) call(Source& source,Args&&... arguments){
    auto value=lvui::native::initial(source);
    using S=typename CallbackPointer<std::remove_cvref_t<decltype(value)>>::signature;
    return ExpressionCall<S>::call([&](auto&&... args)->decltype(auto){return call_function(value,std::forward<decltype(args)>(args)...);},std::forward<Args>(arguments)...);
}
template<class Source,class... Args>decltype(auto) call(ReadContext& context,Source& source,Args&&... arguments){
    auto value=read_source(context,source);
    using S=typename CallbackPointer<std::remove_cvref_t<decltype(value)>>::signature;
    return ExpressionCall<S>::call([&](auto&&... args)->decltype(auto){return read_function(context,value,std::forward<decltype(args)>(args)...);},std::forward<Args>(arguments)...);
}
template<class Source>auto function(ReadContext& context,Source& source){
    auto value=read_source(context,source);
    static_assert(CallbackPointer<std::remove_cvref_t<decltype(value)>>::value,"An inferred event requires a native function value");
    return value;
}
template<class Source>void track(ReadContext& context,Source& source){
    auto value=function(context,source);track_function(context,value);
}
template<class Source,class Factory>ii::core::Result<void> replace(Source& source,Factory factory){return source.replace_native(std::move(factory));}
}
