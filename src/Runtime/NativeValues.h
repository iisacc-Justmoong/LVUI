#pragma once
#include <LVUI/NativeBindings.h>
#include <LVUI/NativeColors.h>
#include <thread>
#include <iiSDKCore/Core/Text/Number.h>
#include <iiSDKCore/Core/Value/Value.h>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <ranges>

namespace lvui::native {
template<class T> concept Numeric = std::is_arithmetic_v<std::remove_cvref_t<T>> &&
    !std::same_as<std::remove_cvref_t<T>, bool>;
template<class T> concept String = std::convertible_to<T, std::string_view>;
template<String T> std::string_view string_view(const T& value) {
    if constexpr (std::is_pointer_v<T>) if (!value) throw std::invalid_argument("String cannot be null");
    return std::string_view(value);
}
template<Numeric T> double number(T value) {
    const double result = static_cast<double>(value);
    if (!std::isfinite(result)) throw std::domain_error("Number must be finite");
    return result;
}
inline double number(const ii::core::Value& value){
    return std::visit([](const auto& item)->double{
        if constexpr(Numeric<decltype(item)>)return number(item);
        else throw std::invalid_argument("Core numeric input requires a numeric Value");
    },value.storage());
}
inline double checked(double value) { return number(value); }
template<Numeric T> double range(T value, double low, double high) {
    const double result = number(value);
    if (result < low || result > high) throw std::out_of_range("Number outside input range");
    return result;
}
inline double range(const ii::core::Value& value,double low,double high){return range(number(value),low,high);}
inline bool truth(bool value) { return value; }
inline bool truth(const ii::core::Value& value){auto flag=value.get<bool>();if(!flag)throw std::invalid_argument("Core Bool input requires a boolean Value");return flag->get();}
template<class T> requires (!std::same_as<std::remove_cvref_t<T>, bool>) bool truth(T) = delete;
template<Numeric T> std::int64_t integer(T value) {
    const double n = number(value);
    if (std::trunc(n) != n || std::abs(n) > 9007199254740991.0)
        throw std::domain_error("Int must be an exactly representable integer");
    return static_cast<std::int64_t>(n);
}
inline std::int64_t integer(const ii::core::Value& value){return integer(number(value));}
template<Numeric A, Numeric B> double add(A a, B b) { return checked(number(a) + number(b)); }
template<String A, String B> std::string add(const A& a, const B& b) {
    const auto left = lvui::native::string_view(a), right = lvui::native::string_view(b);
    if (left.size() > 1048576 || right.size() > 1048576 - left.size())
        throw std::length_error("String expression exceeds 1 MiB");
    return std::string(left) + std::string(right);
}
template<Numeric A, Numeric B> double subtract(A a, B b) { return checked(number(a) - number(b)); }
template<Numeric A, Numeric B> double multiply(A a, B b) { return checked(number(a) * number(b)); }
template<Numeric A, Numeric B> double divide(A a, B b) {
    const double divisor = number(b);
    if (divisor == 0) throw std::domain_error("Division by zero");
    return checked(number(a) / divisor);
}
template<Numeric A, Numeric B> double remainder(A a, B b) {
    const double divisor = number(b);
    if (divisor == 0) throw std::domain_error("Remainder by zero");
    return checked(std::fmod(number(a), divisor));
}
template<Numeric T> double negate(T value) { return checked(-number(value)); }
template<Numeric A, Numeric B> bool equal(A a, B b) { return number(a) == number(b); }
template<String A, String B> bool equal(const A& a, const B& b) { return lvui::native::string_view(a) == lvui::native::string_view(b); }
template<class A, class B> requires (std::same_as<A, bool> && std::same_as<B, bool>)
bool equal(A a, B b) { return a == b; }
template<EncodedColor A,EncodedColor B> bool equal(const A& a,const B& b){return color_channels(a)==color_channels(b);}
template<Numeric A, Numeric B> bool less(A a, B b) { return number(a) < number(b); }
template<String A, String B> bool less(const A& a, const B& b) { return lvui::native::string_view(a) < lvui::native::string_view(b); }
template<class T> auto value_copy(T&& value) { return detail::snapshot(std::forward<T>(value)); }
template<EncodedColor To,class T> auto widget_value(T&& value){
    if constexpr(EncodedColor<std::remove_cvref_t<T>>)return color<To>(value);
    else return value_copy(std::forward<T>(value));
}
template<class Source,class Value>ii::core::Result<void> assign_value(Source& source,Value&& value){
    if constexpr(EncodedColor<std::remove_cvref_t<Value>>)(void)color_channels(value);
    using Result=decltype(source.set(std::forward<Value>(value)));
    if constexpr(std::same_as<Result,ii::core::Result<void>>)return source.set(std::forward<Value>(value));
    else{
        static_assert(std::same_as<Result,bool>||std::same_as<Result,void>,"A native value setter must return a change flag, void or Result<void>");
        (void)source.set(std::forward<Value>(value));return {};
    }
}
template<class First,class... Rest> auto list(First&& first,Rest&&... rest){
    using T=detail::StoredValue<First>;
    if constexpr(String<const First&>&&(String<const Rest&>&&...))
        return std::vector<std::string>{std::string(lvui::native::string_view(first)),std::string(lvui::native::string_view(rest))...};
    else if constexpr(Numeric<First>&&(Numeric<Rest>&&...)){
        if constexpr((std::same_as<std::remove_cvref_t<First>,std::remove_cvref_t<Rest>>&&...)){
            if constexpr(std::floating_point<std::remove_cvref_t<First>>){(void)number(first);((void)number(rest),...);}
            return std::vector<T>{value_copy(std::forward<First>(first)),value_copy(std::forward<Rest>(rest))...};
        }else return std::vector<double>{number(first),number(rest)...};
    }else{
        static_assert((std::same_as<T,detail::StoredValue<Rest>>&&...),"List elements require compatible native value types");
        return std::vector<T>{value_copy(std::forward<First>(first)),value_copy(std::forward<Rest>(rest))...};
    }
}
template<class Then, class Else> auto choose(bool test, Then then_value, Else else_value) {
    using A = detail::StoredValue<std::invoke_result_t<Then>>;
    using B = detail::StoredValue<std::invoke_result_t<Else>>;
    static_assert((Numeric<A> && Numeric<B>) || (EncodedColor<A> && EncodedColor<B>) || std::same_as<A, B>, "Conditional branches require compatible types");
    if constexpr (Numeric<A> && Numeric<B>) return test ? number(then_value()) : number(else_value());
    else if constexpr(EncodedColor<A> && EncodedColor<B>)return test?color_channels(then_value()):color_channels(else_value());
    else return test ? value_copy(then_value()) : value_copy(else_value());
}
template<std::ranges::random_access_range R, Numeric N> auto index(const R& range, N offset) {
    const auto n = integer(offset);
    if (n < 0 || static_cast<std::uint64_t>(n) >= std::ranges::size(range))
        throw std::out_of_range("Index outside list bounds");
    return value_copy(range[static_cast<std::size_t>(n)]);
}
inline std::string text(const ii::core::Value& value){
    return std::visit([](const auto& item)->std::string{
        using T=std::decay_t<decltype(item)>;
        if constexpr(std::same_as<T,std::monostate>)return {};
        else if constexpr(std::same_as<T,std::string>)return item;
        else if constexpr(std::same_as<T,bool>)return item?"true":"false";
        else if constexpr(std::integral<T>)return std::to_string(item);
        else if constexpr(std::floating_point<T>)return ii::core::format_decimal(item);
        else throw std::invalid_argument("Label requires a scalar Core Value");
    },value.storage());
}
inline std::string text(const std::optional<ii::core::Value>& value){return value?text(*value):std::string{};}
template<class Map,String Key>requires requires(const Map& map,const std::string& key){map.find(key);map.end();map.find(key)->second;}
auto index(const Map& map,const Key& key){
    auto found=map.find(std::string(lvui::native::string_view(key)));
    if(found==map.end())throw std::out_of_range("Native map key was not found");
    return value_copy(found->second);
}
// Core containers remain owned native data. Copy the selected value before an
// rvalue container expires; lookup never inserts or exposes a borrowed child.
template<String Key>ii::core::Value index(const ii::core::Value& value,const Key& key){
    if(value.kind()!=ii::core::ValueKind::object)throw std::invalid_argument("Core string index requires an object Value");
    auto found=value.find(lvui::native::string_view(key));
    if(!found)throw std::out_of_range("Core object key was not found");
    return found->get();
}
template<Numeric N>ii::core::Value index(const ii::core::Value& value,N offset){
    const auto items=value.get<ii::core::Value::Array>();
    if(!items)throw std::invalid_argument("Core numeric index requires an array Value");
    return index(items->get(),offset);
}
inline ii::core::Value index(const ii::core::Value& value,const ii::core::Value& key){
    return std::visit([&](const auto& item)->ii::core::Value{
        if constexpr(Numeric<decltype(item)>||String<decltype(item)>)return index(value,item);
        else throw std::invalid_argument("Core index requires a text or numeric key Value");
    },key.storage());
}
template<class T> std::string text(const T& value) {
    if constexpr (String<const T&>) return std::string(lvui::native::string_view(value));
    else if constexpr (std::same_as<T, bool>) return value ? "true" : "false";
    else if constexpr (Numeric<T>) {
        if constexpr (std::is_integral_v<T>) return std::to_string(value);
        else return ii::core::format_decimal(number(value));
    } else static_assert(sizeof(T) == 0, "Label requires a scalar value");
}

// C++ access is compiled, never reflected from a string. Native Observable
// members supply their own publication; ordinary public members use the
// generated scope's revision, published after a bound method or by its owner.
template<class T> concept ObservableSource = requires(T& value) {
    value.get(); value.revision(); value.observe([](const auto&, std::uint64_t) {});
};
template<class T> concept ReadableSource = requires(T& value) { value.get(); value.revision(); };
template<class T> decltype(auto) initial(T& value) {
    if constexpr (ReadableSource<T>) return value.get(); else return (value);
}
template<class T>requires(!std::is_lvalue_reference_v<T>&&!ReadableSource<std::remove_reference_t<T>>)
auto initial(T&& value){return value_copy(std::forward<T>(value));}
template<class T>decltype(auto) retained_initial(ReadContext& context,T& value) {
    static_assert(!std::is_const_v<T> || !ObservableSource<std::remove_const_t<T>> || ObservableSource<T>,
        "Readonly native Observable source requires const observe; use an explicit Static snapshot for a fixed value");
    decltype(auto) result=initial(value);context.retain(result);
    if constexpr(std::is_rvalue_reference_v<decltype(result)>)return std::move(result);
    else return result;
}
template<class T, class Revision> decltype(auto) read(ReadContext& context, T& value, Revision& revision) {
    if constexpr (ObservableSource<T>) return context.read(value);
    else { (void)context.read(revision); return retained_initial(context,value); }
}
template<class T> decltype(auto) read_source(ReadContext& context,T& value){
    if constexpr(ObservableSource<T>)return context.read(value);else return retained_initial(context,value);
}
template<class T>requires(!std::is_lvalue_reference_v<T>&&!ReadableSource<std::remove_reference_t<T>>)
auto read_source(ReadContext& context,T&& value){context.retain(value);return value_copy(std::forward<T>(value));}
template<class T,class Revision>requires(!std::is_lvalue_reference_v<T>&&!ReadableSource<std::remove_reference_t<T>>)
auto read(ReadContext& context,T&& value,Revision& revision){(void)context.read(revision);context.retain(value);return value_copy(std::forward<T>(value));}
namespace detail {
template<class>inline constexpr bool member_reference=false;
template<class T>inline constexpr bool member_reference<std::reference_wrapper<T>> = true;
template<class T>concept NamedData=std::same_as<std::remove_cvref_t<T>,ii::core::Value>||requires(const std::remove_cvref_t<T>& value){value.find(std::string{});value.end();value.find(std::string{})->second;};
}
template<class Object, class Selector> decltype(auto) member(Object&& object, Selector selector) {
    using T = std::remove_cvref_t<Object>;
    if constexpr(detail::member_reference<T>)return member(object.get(),std::move(selector));
    else if constexpr (std::is_pointer_v<T> || requires { object.get(); *object; bool(object); }) {
        if (!object) throw std::logic_error("Native member path requires a live object");
        return member(*object, std::move(selector));
    } else return std::invoke(selector, std::forward<Object>(object));
}
template<class Object,class Selector>decltype(auto) member(ReadContext& context,Object&& object,Selector selector) {
    using T=std::remove_cvref_t<Object>;context.retain(object);
    if constexpr(detail::member_reference<T>)return member(context,object.get(),std::move(selector));
    else if constexpr(std::is_pointer_v<T>||requires{object.get();*object;bool(object);}){
        if(!object)throw std::logic_error("Native member path requires a live object");
        return member(context,*object,std::move(selector));
    }else return std::invoke(selector,std::forward<Object>(object));
}
// A named property keeps native C++ member selection while Core/string-key maps
// use owned, non-inserting data lookup. The selector is never instantiated for maps.
template<class Object,class Selector>decltype(auto) named_member(Object&& object,std::string_view key,Selector selector){
    using T=std::remove_cvref_t<Object>;
    if constexpr(detail::member_reference<T>)return named_member(object.get(),key,std::move(selector));
    else if constexpr(std::is_pointer_v<T>||requires{object.get();*object;bool(object);}){
        if(!object)throw std::logic_error("Native member path requires a live object");
        return named_member(*object,key,std::move(selector));
    }else if constexpr(detail::NamedData<T>)return index(object,key);
    else return std::invoke(selector,std::forward<Object>(object));
}
template<class Object,class Selector>decltype(auto) named_member(ReadContext& context,Object&& object,std::string_view key,Selector selector){
    context.retain(object);
    using T=std::remove_cvref_t<Object>;
    if constexpr(detail::member_reference<T>)return named_member(context,object.get(),key,std::move(selector));
    else if constexpr(std::is_pointer_v<T>||requires{object.get();*object;bool(object);}){
        if(!object)throw std::logic_error("Native member path requires a live object");
        return named_member(context,*object,key,std::move(selector));
    }else if constexpr(detail::NamedData<T>)return index(object,key);
    else return std::invoke(selector,std::forward<Object>(object));
}
template<class Function> decltype(auto) invoke(ii::core::Observable<std::uint64_t>& revision, Function function) {
    struct Publish final {
        ii::core::Observable<std::uint64_t>& source;
        ~Publish() noexcept(false) {
            const auto publish = [&] {
                if (source.get() == UINT64_MAX) throw std::length_error("Backend revision exhausted");
                source.set(source.get() + 1);
            };
            // A throwing method may already have changed public state. Publish
            // it while preserving that method's original exception.
            if (std::uncaught_exceptions()) { try { publish(); } catch (...) {} }
            else publish();
        }
    } publish{revision};
    return function();
}
template<class Function>void batch(Bindings& bindings,Function function){
    const auto check=[](const auto& result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}};
    check(bindings.begin_batch());std::exception_ptr failure;
    try{function();}catch(...){failure=std::current_exception();}
    auto ended=bindings.end_batch();if(failure)std::rethrow_exception(failure);
    if(!ended&&ended.error().code==ii::core::ErrorCode::closed&&!bindings.available())return;
    check(ended);
}
namespace detail {
template<class>struct StandardFunction:std::false_type{};
template<class Signature>struct StandardFunction<std::function<Signature>>:std::true_type{};
#if defined(__cpp_lib_move_only_function)
template<class Signature>struct StandardFunction<std::move_only_function<Signature>>:std::true_type{};
#endif
template<class>struct FunctionReference:std::false_type{};
template<class T>struct FunctionReference<std::reference_wrapper<T>>:std::true_type{};
template<class F>void require_callable(F& function){
    using T=std::remove_cvref_t<F>;
    if constexpr(FunctionReference<T>::value)require_callable(function.get());
    else if constexpr((std::is_pointer_v<T>&&std::is_function_v<std::remove_pointer_t<T>>)||std::is_member_pointer_v<T>||StandardFunction<T>::value)
        if(!function)throw std::invalid_argument("Native callable is empty");
}
template<class Expected,class Actual>decltype(auto) callback_argument(Actual&& value){
    using T=std::remove_cvref_t<Actual>;
    if constexpr(EncodedColor<Expected>){
        static_assert(EncodedColor<T>,"Callback Color requires an encoded sRGB color value");
        return color<Expected>(value);
    }else if constexpr(std::same_as<Expected,std::string>){
        static_assert(String<const T&>,"Callback String input requires native text");
        return std::string(lvui::native::string_view(value));
    }else if constexpr(std::same_as<Expected,bool>){
        static_assert(std::same_as<T,bool>,"Callback Bool input requires Bool");return bool(value);
    }else if constexpr(std::same_as<Expected,std::int64_t>){
        static_assert(std::integral<T>&&!std::same_as<T,bool>,"Callback Int input requires an integral event payload");return integer(value);
    }else if constexpr(std::floating_point<Expected>){
        static_assert(Numeric<T>,"Callback numeric input requires a numeric event payload");
        const auto checked=number(value);
        if(checked>std::numeric_limits<Expected>::max()||checked<std::numeric_limits<Expected>::lowest())throw std::out_of_range("Callback numeric input outside declared type range");
        return static_cast<Expected>(checked);
    }else{
        static_assert(std::same_as<Expected,T>,"Callback event does not match its declared C++ type");return std::forward<Actual>(value);
    }
}
template<class Expected,class F>Expected function_result(F&& function){
    using Actual=std::invoke_result_t<F&>;
    if constexpr(std::same_as<Expected,void>){
        static_assert(std::same_as<Actual,void>,"A callback declared Void must return void");
        (void)std::invoke(function);
    }else{
        static_assert(!std::same_as<Actual,void>,"A value-returning callable requires a native result");
        if constexpr(!std::same_as<Actual,void>){
            if constexpr(EncodedColor<Expected>||std::same_as<Expected,std::string>||std::same_as<Expected,bool>||std::same_as<Expected,std::int64_t>||std::floating_point<Expected>)
                return callback_argument<Expected>(std::invoke(function));
            else{
                static_assert(std::same_as<Expected,std::remove_cvref_t<Actual>>,"Native callable result does not match its declared type");
                return std::invoke(function);
            }
        }
    }
}
template<class Signature>struct CallbackInvocation;
template<class Returned,class... Expected>struct CallbackInvocation<Returned(Expected...)> final {
    using Result=Returned;
    template<class F,class... Actual>static Returned call(F&& function,Actual&&... arguments){
        static_assert(sizeof...(Expected)==sizeof...(Actual),"Callback event arity does not match its declared signature");
        if constexpr(sizeof...(Expected)==sizeof...(Actual)){
            require_callable(function);
            return function_result<Returned>([&]() -> decltype(auto){return std::invoke(function,callback_argument<Expected>(std::forward<Actual>(arguments))...);});
        }
    }
};
}
// Signatures are generated C++ types. Event adaptation is checked without a
// string registry or callable copies, including move-only factory results.
template<class Signature,class F,class... Actual>auto invoke_callback(F&& function,Actual&&... arguments)->typename detail::CallbackInvocation<Signature>::Result{
    return detail::CallbackInvocation<Signature>::call(std::forward<F>(function),std::forward<Actual>(arguments)...);
}
// Event callbacks borrow their owning activation. A saved callback cannot keep
// an otherwise retired backend alive; an admitted call holds it until return.
template<class Result,class Scope, class Function>
auto scoped_function(const std::shared_ptr<Scope>& scope, const std::shared_ptr<Bindings>& activation, Function function) {
    return [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(activation),
            thread = std::this_thread::get_id(), function = std::move(function)](auto&&... arguments) mutable -> Result {
        const auto rejected=[]() -> Result {if constexpr(std::same_as<Result,void>)return;else throw std::logic_error("Native function owner retired or wrong execution domain");};
        if(thread!=std::this_thread::get_id())return rejected();
        auto active = weak_activation.lock();
        if (!active || !active->available()) return rejected();
        auto owner = weak_scope.lock();
        if (!owner) return rejected();
        if constexpr (requires { owner->active(); }) if (!owner->active()) return rejected();
        if constexpr(std::same_as<Result,void>)(void)std::invoke(function,owner,std::forward<decltype(arguments)>(arguments)...);
        else return std::invoke(function,owner,std::forward<decltype(arguments)>(arguments)...);
    };
}
template<class Scope,class Function>
auto callback(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,Function function){
    return scoped_function<void>(scope,activation,std::move(function));
}
namespace detail {
template<class Scope,class Factory>
auto callable_source(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,Factory factory){
    using Returned=std::invoke_result_t<Factory&,ReadContext&>;
    using Handler=std::remove_reference_t<Returned>;
    static_assert(std::is_object_v<Handler>||(std::is_lvalue_reference_v<Returned>&&std::is_function_v<Handler>),"A callback factory must return an invocable C++ object or function reference");
    if(!scope||!activation||!activation->available())throw std::logic_error("Returned callback requires an active scope");
    auto source=activation->derive([factory=std::move(factory),weak_scope=std::weak_ptr(scope)](ReadContext& context)mutable{
        auto handler=[&]{
            if constexpr(std::is_lvalue_reference_v<Returned>){
                auto owner=weak_scope.lock();if(!owner)throw std::logic_error("Borrowed callable owner retired");
                return std::shared_ptr<Handler>(std::move(owner),std::addressof(std::invoke(factory,context)));
            }else return std::shared_ptr<Handler>(new Handler(std::invoke(factory,context)));
        }();
        require_callable(*handler);return handler;
    });
    if(!source){if(source.error().cause)std::rethrow_exception(source.error().cause);throw std::runtime_error(source.error().message);}
    return *source;
}
}
// The graph closes its source even when external values retain the source's
// identity. An admitted call owns its current handler until it returns.
template<class Scope,class Factory,class Invoke>
auto returned_callback(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,
                       Factory factory,Invoke invoke_handler){
    auto source=detail::callable_source(scope,activation,std::move(factory));
    return callback(scope,activation,[source,invoke_handler=std::move(invoke_handler)](const auto& owner_scope,auto&&... arguments)mutable{
        auto admitted=source->get();
        detail::require_callable(*admitted);
        (void)std::invoke(invoke_handler,owner_scope,*admitted,std::forward<decltype(arguments)>(arguments)...);
    });
}
// Value-returning counterpart: preserve the current dependency-tracked
// callable version and admit calls only while its owning activation is live.
template<class Result,class Scope,class Factory,class Invoke>
auto returned_function(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,
                       Factory factory,Invoke invoke_handler){
    auto source=detail::callable_source(scope,activation,std::move(factory));
    return scoped_function<Result>(scope,activation,[source,invoke_handler=std::move(invoke_handler)](const auto& owner_scope,auto&&... arguments)mutable -> Result{
        auto admitted=source->get();detail::require_callable(*admitted);
        return std::invoke(invoke_handler,owner_scope,*admitted,std::forward<decltype(arguments)>(arguments)...);
    });
}
template<class Signature>class CallbackValue;
template<class Signature>class FunctionVariable;
template<class Signature,class Scope>
std::shared_ptr<CallbackValue<Signature>> snapshot_callback_value(
    const std::shared_ptr<Scope>&,const std::shared_ptr<Bindings>&,
    const std::shared_ptr<CallbackValue<Signature>>&);
template<class Result,class... Arguments>class CallbackValue<Result(Arguments...)> final {
    template<class T>using Input=std::conditional_t<std::is_arithmetic_v<T>||std::same_as<T,std::string>,T,const T&>;
public:
    using Action=std::function<Result(Input<Arguments>...)>;
    using SnapshotFactory=std::function<std::shared_ptr<Action>()>;
    using VersionSubscription=std::function<ii::core::ObservableSubscription(std::function<void()>)>;
    explicit CallbackValue(Action action,std::function<void(ReadContext&)> track={},std::function<bool()> ready={},SnapshotFactory snapshot={},VersionSubscription subscribe={}):state_(std::make_shared<State>(std::move(action),std::move(track),std::move(ready),std::move(snapshot))){
        if(!state_->action)throw std::invalid_argument("Native callback value requires an action");
        if(subscribe)state_->subscription=subscribe([weak=std::weak_ptr(state_)]{
            if(auto state=weak.lock())state->version.set(state->version.get()+1);
        });
    }
    CallbackValue(const CallbackValue&)=delete;CallbackValue& operator=(const CallbackValue&)=delete;
    template<class... Actual>requires(sizeof...(Actual)==sizeof...(Arguments)) Result operator()(Actual&&... arguments)const{
        auto admitted=state_;
        if(!available(admitted))return rejected();
        return invoke_callback<Result(Arguments...)>(admitted->action,std::forward<Actual>(arguments)...);
    }
    template<class... Actual>Result read(ReadContext& context,Actual&&... arguments)const{
        auto admitted=state_;
        if(!available(admitted))return rejected();
        if(admitted->track)admitted->track(context);
        if(!available(admitted))return rejected();
        return invoke_callback<Result(Arguments...)>(admitted->action,std::forward<Actual>(arguments)...);
    }
    void track(ReadContext& context)const{
        auto admitted=state_;
        if(!available(admitted)){if constexpr(std::same_as<Result,void>)return;else throw std::logic_error("Native function value is closed or requires its owner thread");}
        if(admitted->track)admitted->track(context);
    }
    std::uint64_t revision()const{
        auto admitted=state_;if(!available(admitted))throw std::logic_error("Native function version requires an active owner domain");
        return admitted->version.revision();
    }
    ii::core::ObservableSubscription observe(std::function<void(std::uint64_t)> changed,bool initial=false)const{
        auto admitted=state_;if(!available(admitted))throw std::logic_error("Native function observation requires an active owner domain");
        if(!changed)throw std::invalid_argument("Native function observation requires a callback");
        return admitted->version.observe([changed=std::move(changed)](const auto&,std::uint64_t revision){changed(revision);},initial);
    }
    std::shared_ptr<Action> select()const{return capture_current();}
private:
    template<class>friend class FunctionVariable;
    template<class Signature,class Scope>friend std::shared_ptr<CallbackValue<Signature>> snapshot_callback_value(
        const std::shared_ptr<Scope>&,const std::shared_ptr<Bindings>&,
        const std::shared_ptr<CallbackValue<Signature>>&);
    std::shared_ptr<Action> capture_current()const{
        auto admitted=state_;
        if(!available(admitted))throw std::logic_error("A native function snapshot requires an active owner domain");
        if(admitted->snapshot){
            auto selected=admitted->snapshot();
            if(!selected||!*selected)throw std::invalid_argument("The selected native callable is empty");
            return selected;
        }
        return std::make_shared<Action>([admitted](Input<Arguments>... arguments)->Result{
            if(!available(admitted))return rejected();
            return std::invoke(admitted->action,std::forward<Input<Arguments>>(arguments)...);
        });
    }
    struct State final {
        State(Action a,std::function<void(ReadContext&)> t,std::function<bool()> r,SnapshotFactory s):action(std::move(a)),track(std::move(t)),ready(std::move(r)),snapshot(std::move(s)){}
        const std::thread::id thread=std::this_thread::get_id();
        Action action;
        std::function<void(ReadContext&)> track;
        std::function<bool()> ready;
        SnapshotFactory snapshot;
        ii::core::Observable<std::uint64_t> version{0};
        ii::core::ObservableSubscription subscription;
    };
    static Result rejected(){if constexpr(std::same_as<Result,void>)return;else throw std::logic_error("Native function value is closed or requires its owner thread");}
    static bool available(const std::shared_ptr<State>& state){return state->thread==std::this_thread::get_id()&&(!state->ready||state->ready());}
    bool available()const{auto admitted=state_;return available(admitted);}
    std::shared_ptr<State> state_;
};
template<class Value,class... Arguments>auto call_function(const Value& value,Arguments&&... arguments)->decltype((*value)(std::forward<Arguments>(arguments)...)){
    if(!value)throw std::logic_error("Native function value not initialized");return (*value)(std::forward<Arguments>(arguments)...);
}
template<class Value,class... Arguments>auto read_function(ReadContext& context,const Value& value,Arguments&&... arguments)->decltype(value->read(context,std::forward<Arguments>(arguments)...)){
    if(!value)throw std::logic_error("Native function value not initialized");return value->read(context,std::forward<Arguments>(arguments)...);
}
template<class Value>void track_function(ReadContext& context,const Value& value){
    if(!value)throw std::logic_error("Native function value not initialized");value->track(context);
}
// A component owns one typed callable value. Consumers share the value, while
// the activation graph owns its current move-only handler version.
template<class Signature,class Scope,class Factory,class Invoke>
auto make_callback_value(const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,
                         Factory factory,Invoke invoke_handler){
    using Result=typename detail::CallbackInvocation<Signature>::Result;
    using Returned=std::invoke_result_t<Factory&,ReadContext&>;
    using Action=typename CallbackValue<Signature>::Action;
    auto source=detail::callable_source(scope,activation,std::move(factory));
    auto invoke=std::make_shared<Invoke>(std::move(invoke_handler));
    auto action=scoped_function<Result>(scope,activation,[source,invoke](const auto& owner,auto&&... arguments) -> Result{
        auto admitted=source->get();
        detail::require_callable(*admitted);
        return detail::function_result<Result>([&]() -> decltype(auto){return std::invoke(*invoke,owner,*admitted,std::forward<decltype(arguments)>(arguments)...);});
    });
    auto ready=[weak_scope=std::weak_ptr(scope),weak_graph=std::weak_ptr(activation)]{
        auto graph=weak_graph.lock();if(!graph||!graph->available())return false;
        auto owner=weak_scope.lock();if(!owner)return false;
        if constexpr(requires{owner->active();})return owner->active();else return true;
    };
    auto snapshot=[weak_scope=std::weak_ptr(scope),weak_graph=std::weak_ptr(activation),source,invoke]() -> std::shared_ptr<Action>{
        auto graph=weak_graph.lock();if(!graph||!graph->available())throw std::logic_error("Native function snapshot owner retired");
        auto owner=weak_scope.lock();if(!owner)throw std::logic_error("Native function snapshot receiver retired");
        // A borrowed handler is protected by scoped_function's admitted owner,
        // not by retaining the factory source's aliasing shared_ptr forever.
        auto selected=[&]{
            if constexpr(std::is_lvalue_reference_v<Returned>)return source->get().get();
            else return source->get();
        }();
        return std::make_shared<Action>(scoped_function<Result>(owner,graph,[selected,invoke](const auto& owner,auto&&... arguments) -> Result{
            detail::require_callable(*selected);
            return detail::function_result<Result>([&]() -> decltype(auto){return std::invoke(*invoke,owner,*selected,std::forward<decltype(arguments)>(arguments)...);});
        }));
    };
    return std::make_shared<CallbackValue<Signature>>(std::move(action),[source](ReadContext& context){(void)context.read(*source);},std::move(ready),std::move(snapshot),
        [source](std::function<void()> changed){return source->observe([changed=std::move(changed)](const auto&,std::uint64_t){changed();});});
}
// A frozen value owns one selected handler version through its graph. Its
// source receiver stays borrowed and is gated again for each admitted call.
template<class Signature,class Scope>
std::shared_ptr<CallbackValue<Signature>> snapshot_callback_value(
    const std::shared_ptr<Scope>& scope,const std::shared_ptr<Bindings>& activation,
    const std::shared_ptr<CallbackValue<Signature>>& value){
    if(!scope||!activation||!activation->available()||!value)
        throw std::logic_error("A native function snapshot requires an active scope and value");
    auto selected=value->capture_current();
    using Result=typename detail::CallbackInvocation<Signature>::Result;
    return make_callback_value<Signature>(scope,activation,[selected](ReadContext&){return selected;},
        [](const auto&,auto& handler,auto&&... arguments) -> Result{
            return std::invoke(*handler,std::forward<decltype(arguments)>(arguments)...);
        });
}
} // namespace lvui::native
