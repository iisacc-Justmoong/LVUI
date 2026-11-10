#pragma once
#include <LVUI/NativeTab.h>
#include <LVUI/NativeSection.h>
#include <functional>
#include <vector>
namespace lvui::native {
template<class Widget,class Snapshot>using TabRegistrationPortValue=std::function<ii::core::Result<void>(TabGroupPortValue<Widget,Snapshot>)>;
template<class Widget,class Snapshot>using TabSectionPortValue=std::function<ii::core::Result<void>(std::vector<TabGroupPortValue<Widget,Snapshot>>)>;
template<class Widget,class Snapshot>using TabCaptureValue=SectionCapture<TabGroupPortValue<Widget,Snapshot>>;
template<class Widget,class Snapshot>using TabRepeatedRowsValue=SectionRepeatedRows<TabGroupPortValue<Widget,Snapshot>>;
template<class Widget,class Snapshot>struct TabSectionAccessValue final {
 std::function<ii::core::Result<TabSectionPortValue<Widget,Snapshot>>(Widget)> register_section;std::function<ii::core::Result<TabBarConfiguration>()> options;
};
template<class NativeModel,class Widget,class Snapshot>struct TabBarHostPolicy final {
 using Model=NativeModel;using Configuration=TabBarConfiguration;using GroupPort=TabGroupPortValue<Widget,Snapshot>;using Presentation=TabPresentation;
 using FocusMove=TabFocusMove;using Activation=TabActivation;using RegistrationPort=TabRegistrationPortValue<Widget,Snapshot>;
 using SectionPort=TabSectionPortValue<Widget,Snapshot>;using SectionAccess=TabSectionAccessValue<Widget,Snapshot>;
};
template<template<class>class NativeModel,template<class>class NativeHost,class Widget,class Snapshot>
using TabBarBinding=NativeHost<TabBarHostPolicy<NativeModel<TabBarModelPolicy>,Widget,Snapshot>>;
} // namespace lvui::native
