#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeVisual.h>
#include <LVUI/TableHeader.h>
#include <string>
#include <string_view>
namespace lvui::native {
template<class Decoder>struct TableHeaderPresentationPolicy final {
    using Column=TableColumn;using Appearance=TableHeaderPresentation;using ColumnAppearance=TableHeaderColumnAppearance;
    using IconSource=::lvui::IconSource;using IconCache=NativeIconCache<Decoder>;
    static constexpr std::size_t max_columns=table_maximum_columns;
    [[nodiscard]] static bool valid_appearance(const Appearance& value){return valid_table_header_presentation(value);}
    [[nodiscard]] static std::string display_text(std::string_view text,std::size_t limit){return native_display_text(text,limit);}
};
template<template<class>class Engine,class Decoder>
using TableHeaderPresentationBinding=Engine<TableHeaderPresentationPolicy<Decoder>>;
} // namespace lvui::native
