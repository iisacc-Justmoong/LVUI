#pragma once
#include <LVUI/Menu.h>
#include <LVUI/NativeMenuItem.h>
#include <LVUI/NativeSection.h>
#include <functional>
#include <optional>
#include <utility>
#include <vector>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor menu_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct MenuInput final {MenuConfiguration configuration;int selected_index=-1;friend bool operator==(const MenuInput&,const MenuInput&)=default;};
struct NativeMenuCallbacks final {std::function<void(int)> triggered;std::function<void()> opened,closed,canceled;};
template<class Widget,class Snapshot>using MenuRegistrationPortValue=std::function<ii::core::Result<void>(MenuItemPortValue<Widget,Snapshot>)>;
template<class Widget,class Snapshot>using MenuSectionPortValue=std::function<ii::core::Result<void>(std::vector<MenuItemPortValue<Widget,Snapshot>>)>;
template<class Widget,class Snapshot>struct MenuSectionAccessValue final {
    std::function<ii::core::Result<MenuSectionPortValue<Widget,Snapshot>>(Widget)> register_section;
    std::function<ii::core::Result<MenuInput>()> options;
};
template<class Widget,class Snapshot>using MenuCaptureValue=SectionCapture<MenuItemPortValue<Widget,Snapshot>>;
template<class Widget,class Snapshot>using MenuRepeatedRowsValue=SectionRepeatedRows<MenuItemPortValue<Widget,Snapshot>>;
template<class NativeModel,class Widget,class Snapshot>struct MenuHostPolicy final {
    using Model=NativeModel;using Input=MenuInput;using Callbacks=NativeMenuCallbacks;
    using ItemPort=MenuItemPortValue<Widget,Snapshot>;using Presentation=MenuItemPresentation;
    using CloseAction=MenuCloseAction;using ClosePlan=MenuClosePlan;using Activation=MenuItemActivation;
    using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuEntryState;using Configuration=MenuConfiguration;
    using Layout=MenuLayout;using Color=StyleColor;using Error=MenuError;using ErrorCode=MenuErrorCode;using Navigation=MenuNavigation;
    using Direction=MenuDirection;using PopupPort=MenuPopupPortValue<Widget,Snapshot>;
    using SectionPort=MenuSectionPortValue<Widget,Snapshot>;using SectionAccess=MenuSectionAccessValue<Widget,Snapshot>;
    using RegistrationPort=MenuRegistrationPortValue<Widget,Snapshot>;
};
template<template<class>class NativeModel,template<class>class NativeHost,class Widget,class Snapshot>
using MenuBinding=NativeHost<MenuHostPolicy<NativeModel<MenuModelPolicy>,Widget,Snapshot>>;
} // namespace lvui::native
