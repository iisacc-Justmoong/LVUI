#pragma once
#include <LVUI/PageRouter.h>
#include <functional>
#include <memory>
#include <utility>
namespace lvui::native {
// Applications provide C++ page factories; the widget builder type is injected.
template<class Builder>class NativeNavigationPageValue:public NavigationPage {
public:
    [[nodiscard]] virtual ii::core::Result<void> build(Builder&,const NavigationEntry&,const std::shared_ptr<NavigationContext>&)const=0;
};
template<class Builder>using NativePageFactoryValue=std::function<ii::core::Result<void>(Builder&,const NavigationEntry&,const std::shared_ptr<NavigationContext>&)>;
template<class Builder>class FunctionalNavigationPageValue final:public NativeNavigationPageValue<Builder> {
    NativePageFactoryValue<Builder> factory_;
public:
    explicit FunctionalNavigationPageValue(NativePageFactoryValue<Builder> factory):factory_(std::move(factory)){}
    [[nodiscard]] ii::core::Result<void> build(Builder& builder,const NavigationEntry& entry,const std::shared_ptr<NavigationContext>& context)const override {
        if(!factory_)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Page factory is unavailable");
        return factory_(builder,entry,context);
    }
};
template<class Builder>struct PageRouterHostPolicy final {
    using Page=NativeNavigationPageValue<Builder>;using Entry=NavigationEntry;using Context=NavigationContext;
    using Presenter=NavigationPresenter;using Router=PageRouter;
};
template<template<class>class NativeHost,class Builder>
using PageRouterBinding=NativeHost<PageRouterHostPolicy<Builder>>;
} // namespace lvui::native
