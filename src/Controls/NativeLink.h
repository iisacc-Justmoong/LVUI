#pragma once
#include <LVUI/Link.h>
#include <LVUI/NativeButton.h>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>
namespace lvui::native {
template<class Color>inline StyleColor link_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
inline ButtonTone link_tone(ButtonTone value)noexcept{return value;}
template<class Tone> requires (!std::same_as<Tone,ButtonTone>)
inline ButtonTone link_tone(Tone value)noexcept{return button_tone(value);}
template<class T> requires (!std::is_const_v<T> && std::derived_from<T,NavigationTarget>)
inline std::weak_ptr<NavigationTarget> navigation_target(const std::shared_ptr<T>& value)noexcept{return value;}
template<class T> requires (!std::is_const_v<T> && std::derived_from<T,NavigationTarget>)
inline std::weak_ptr<NavigationTarget> navigation_target(const std::weak_ptr<T>& value)noexcept{return value;}
inline std::weak_ptr<NavigationTarget> navigation_target(std::nullptr_t)noexcept{return {};}
template<class T> requires std::derived_from<std::remove_const_t<T>,NavigationPage>
inline std::shared_ptr<const NavigationPage> navigation_page(const std::shared_ptr<T>& value)noexcept{return value;}
inline std::shared_ptr<const NavigationPage> navigation_page(std::nullptr_t)noexcept{return {};}

struct LinkCallbacks final {
    LinkMethod method;
    std::vector<LinkMethod> methods;
    std::function<void()> clicked,pressed,released,canceled;
    std::function<void(bool)> navigated;
    std::function<void(const ii::core::Error&)> navigation_failed;
};
template<class NativeModel>struct LinkHostPolicy final {
    using Model=NativeModel;using Options=LinkConfiguration;using Context=NavigationContext;using Callbacks=LinkCallbacks;
    using Event=LinkEvent;using Method=LinkMethod;using Tone=ButtonTone;using Color=StyleColor;
};
template<template<class>class NativeModel,template<class>class NativeHost>
using LinkBinding=NativeHost<LinkHostPolicy<NativeModel<LinkModelPolicy>>>;
} // namespace lvui::native
