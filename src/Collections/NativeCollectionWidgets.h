#pragma once
#include <LVUI/NativeCollectionNavigation.h>
#include <functional>
#include <string>
#include <vector>
namespace lvui::widgets {
struct CollectionNavigationEvents final {
    std::function<void(std::string)> current_changed,activated;
    std::function<void(std::vector<std::string>)> selection_changed;
};
template<class T,class Nav,class NativeViewport>struct CollectionWidgetPolicy final {
    using Item=T;using Navigation=Nav;using Frame=lvui::native::VirtualCollectionFrame<T>;
    using Viewport=NativeViewport;using Events=CollectionNavigationEvents;
    using Selection=lvui::native::CollectionSelectionState;using Modifiers=lvui::native::CollectionModifiers;
    using Reply=lvui::native::CollectionNavigationReply;using Command=lvui::native::CollectionCommand;
    using Layout=lvui::native::CollectionNavigationLayout;
};
template<template<class>class Host,class T,class Navigation,class Viewport>
using CollectionWidgetNavigationBinding=Host<CollectionWidgetPolicy<T,Navigation,Viewport>>;
} // namespace lvui::widgets
