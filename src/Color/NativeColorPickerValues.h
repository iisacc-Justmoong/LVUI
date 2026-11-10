#pragma once
#include <LVUI/ColorPickerSession.h>
#include <LVUI/NativeInputField.h>
#include <cstdint>
#include <memory>
namespace lvui::native {
struct ColorPickerValuesInput final {
 std::shared_ptr<ColorPickerSession> session;std::int64_t source_revision=0;double width=296;bool enabled=true;
 friend bool operator==(const ColorPickerValuesInput&,const ColorPickerValuesInput&)=default;
};
template<class NativeField>struct ColorPickerValuesPolicy final {
 using Input=ColorPickerValuesInput;using Snapshot=ColorSelectionSnapshot;using Color=StyleColor;using Field=NativeField;
 using FieldConfiguration=InputFieldConfiguration;using FieldCallbacks=NativeInputFieldCallbacks;using Picker=ColorPicker;
 using SurfaceErrorCode=ColorSurfaceErrorCode;using SelectionErrorCode=ColorSelectionErrorCode;
 using SelectionError=ColorSelectionError;using Channel=ColorChannel;using SelectionResult=ColorSelectionResult<std::uint64_t>;
 [[nodiscard]] static bool valid_color(Color value)noexcept{return valid_color_value(value);}
};
template<template<class>class NativeHost,class NativeField>
using ColorPickerValuesBinding=NativeHost<ColorPickerValuesPolicy<NativeField>>;
} // namespace lvui::native
