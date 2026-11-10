#pragma once

#include <LVUI/Button.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace lvui::native {

template<class Widget>struct ButtonEventValue final {
    Widget source;
    std::string trigger="clicked";
    bool enabled=true,effective_enabled=true;
    std::optional<ButtonTone> tone;
};
template<class Event>using ButtonMethodValue=std::function<void(const Event&)>;
inline constexpr std::size_t maximum_button_methods=256;

namespace detail {
template<class T>struct optional_value:std::false_type{};
template<class T>struct optional_value<std::optional<T>>:std::true_type{};
template<class T>struct variant_value:std::false_type{};
template<class... T>struct variant_value<std::variant<T...>>:std::true_type{};
template<class T>struct shared_value:std::false_type{};
template<class T>struct shared_value<std::shared_ptr<T>>:std::true_type{using object=T;};
template<class T>struct function_value:std::false_type{};
template<class R,class... T>struct function_value<std::function<R(T...)>>:std::true_type{};
template<class>inline constexpr bool unsupported_method=false;

template<class Event,class T>concept button_callable=std::invocable<T&,const Event&>||std::invocable<T&>||
    requires(T& value,const Event& event){value.invoke(event);}||requires(T& value){value.invoke();}||
    requires(T& value,const Event& event){value.trigger(event);}||requires(T& value){value.trigger();};

template<class Event,class T>void invoke_button_candidate(T& value,const Event& event){
    if constexpr(std::invocable<T&,const Event&>)(void)std::invoke(value,event);
    else if constexpr(std::invocable<T&>)(void)std::invoke(value);
    else if constexpr(requires{value.invoke(event);})(void)value.invoke(event);
    else if constexpr(requires{value.invoke();})(void)value.invoke();
    else if constexpr(requires{value.trigger(event);})(void)value.trigger(event);
    else (void)value.trigger();
}

template<class Event,class T>void append_button_methods(std::vector<ButtonMethodValue<Event>>& result,const T& value){
    using V=std::remove_cvref_t<T>;
    if constexpr(std::same_as<V,std::nullptr_t>){return;}
    else if constexpr(optional_value<V>::value){if(value)append_button_methods<Event>(result,*value);}
    else if constexpr(variant_value<V>::value){std::visit([&](const auto& item){append_button_methods<Event>(result,item);},value);}
    else if constexpr(shared_value<V>::value){
        if(!value)return;
        if constexpr(button_callable<Event,typename shared_value<V>::object>){
            if(result.size()>=maximum_button_methods)throw std::length_error("Too many LVUI Button methods");
            result.emplace_back([owner=value](const Event& event){invoke_button_candidate(*owner,event);});
        }else append_button_methods<Event>(result,*value);
    }
    else if constexpr(std::same_as<V,ButtonMethodValue<Event>>){
        if(!value)return;if(result.size()>=maximum_button_methods)throw std::length_error("Too many LVUI Button methods");result.push_back(value);
    }else if constexpr(button_callable<Event,V>){
        static_assert(std::copy_constructible<V>,"Button methods must be copyable");
        if constexpr(std::is_pointer_v<V>||function_value<V>::value)if(!value)return;
        if(result.size()>=maximum_button_methods)throw std::length_error("Too many LVUI Button methods");
        result.emplace_back([candidate=V(value)](const Event& event)mutable{invoke_button_candidate(candidate,event);});
    }else if constexpr(std::ranges::input_range<V>){
        for(const auto& item:value)append_button_methods<Event>(result,item);
    }else static_assert(unsupported_method<V>,"Button methods must be callbacks, optional callbacks, or nested ranges");
}
} // namespace detail

template<class Event,class T>[[nodiscard]] std::vector<ButtonMethodValue<Event>> collect_button_methods(const T& value){
    std::vector<ButtonMethodValue<Event>> result;detail::append_button_methods<Event>(result,value);return result;
}

template<class Widget>[[nodiscard]] inline bool button_method_active(const Widget& root){
    auto interaction=root.interaction();
    if(!interaction||!interaction->alive||!interaction->enabled||!interaction->visible||interaction->input_blocked)return false;
    auto action=root.action_options();return action&&action->enabled&&action->interaction_enabled;
}

template<class Event>class ButtonMethodRegistryValue final {
    using Method=ButtonMethodValue<Event>;
    Method method_;
    std::vector<Method> methods_;
public:
    using event_type=Event;
    [[nodiscard]] bool has_methods()const noexcept{
        return bool(method_)||std::ranges::any_of(methods_,[](const auto& value){return bool(value);});
    }
    void set_method(Method method)noexcept{method_=std::move(method);}
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<Method> methods){
        std::erase_if(methods,[](const auto& value){return !value;});
        if(methods.size()>maximum_button_methods)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Too many LVUI Button methods");
        methods_=std::move(methods);return {};
    }
    [[nodiscard]] ii::core::Result<void> set_registry(Method method,std::vector<Method> methods){
        std::erase_if(methods,[](const auto& value){return !value;});
        if(methods.size()>maximum_button_methods)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Too many LVUI Button methods");
        method_=std::move(method);methods_=std::move(methods);return {};
    }
    template<class Active>[[nodiscard]] bool invoke(const Event& event,Active&& active)const{
        auto method=method_;auto methods=methods_;
        if(method){if(!active())return false;method(event);}
        for(auto& item:methods)if(item){if(!active())return false;item(event);}
        return active();
    }
};

template<class Widget,class Registry>[[nodiscard]] inline ii::core::Result<void> invoke_button_methods(const Widget& root,
        const Registry& methods,bool enabled,std::optional<ButtonTone> tone,
        std::string_view trigger,std::string_view fallback="clicked"){
    auto snapshot=root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
    bool effective=snapshot->enabled;
    if(tone){
        auto options=root.action_options();if(!options)return std::unexpected(options.error());
        effective=effective&&options->enabled&&options->interaction_enabled;
    }
    try{
        const typename Registry::event_type event{root,std::string(trigger.empty()?fallback:trigger),enabled,effective,tone};
        (void)methods.invoke(event,[&]{return root.snapshot().has_value();});return {};
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Button methods failed",std::current_exception());}
}

struct ButtonMethodInvoker final {
    template<class Widget,class Registry>[[nodiscard]] static ii::core::Result<void> invoke(const Widget& root,
            const Registry& methods,bool enabled,std::string_view trigger){
        return invoke_button_methods(root,methods,enabled,std::nullopt,trigger,"manual");
    }
};
} // namespace lvui::native
