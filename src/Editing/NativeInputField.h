#pragma once
#include <LVUI/InputField.h>
#include <LVUI/NativeTextFieldMaterial.h>
#include <LVUI/NativeTextBoundary.h>
#include <LVUI/TextReplacementCursor.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor input_field_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct NativeInputFieldCallbacks final {
 std::function<void(std::string)> edited;std::function<void()> submitted;std::function<void(bool)> focus_changed;
 std::function<void(std::string)> displayed_changed;std::function<void(double)> content_height_changed;
};
struct NativeInputFieldClipboard final {std::function<ii::core::Result<void>(std::string)> write;};
template<class Affinity>struct NativeInputFieldSnapshotValue final {
 std::string text;TextInputRange selection;std::optional<TextInputRange> marked;std::size_t anchor=0,caret=0;
 double horizontal_scroll=0,vertical_scroll=0;Affinity affinity=Affinity::downstream;bool wrapping_fallback=false;double content_height=0;
};
struct InputFieldDefaults final {
 inline static constexpr auto icon_size=search_field_defaults::icon_size,gap=search_field_defaults::gap;
 inline static constexpr auto cylinder_radius=search_field_defaults::cylinder_radius,radius=search_field_defaults::radius;
 inline static constexpr auto font_size=search_field_defaults::font_size,clear_line_height=search_field_defaults::clear_line_height;
 inline static constexpr auto text=search_field_defaults::text,disabled_text=search_field_defaults::disabled_text;
 inline static constexpr auto description=search_field_defaults::description,background=search_field_defaults::background;
};
template<class NativeModel,class NativeMaterial,class Affinity>
struct InputFieldHostPolicy final {
 using Configuration=InputFieldConfiguration;using Palette=InputFieldPalette;using Surface=InputFieldSurface;using Color=StyleColor;
 using TextInput=NativeModel;using Range=TextInputRange;using Error=TextInputError;using ErrorCode=TextInputErrorCode;
 using Callbacks=NativeInputFieldCallbacks;using Clipboard=NativeInputFieldClipboard;using Snapshot=NativeInputFieldSnapshotValue<Affinity>;
 using Material=NativeMaterial;using MaterialVariant=text_field_material::Variant;using Defaults=InputFieldDefaults;
 [[nodiscard]] static bool valid_configuration(const Configuration& value)noexcept{return valid_input_field_configuration(value);}
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
 template<class Index>[[nodiscard]] static auto boundary(std::string_view value){return native_text_boundary<Index>(value);}
 [[nodiscard]] static std::size_t replacement_cursor(std::size_t start,std::size_t end,std::size_t length,std::int32_t offset)noexcept{
  return lvui::text_replacement_cursor(start,end,length,offset);}
};
template<template<class>class NativeHost,template<class>class NativeModel,template<class>class NativeMaterial,class Affinity>
using InputFieldBinding=NativeHost<InputFieldHostPolicy<NativeModel<TextInputModelPolicy>,NativeMaterial<text_field_material::Policy>,Affinity>>;
} // namespace lvui::native
