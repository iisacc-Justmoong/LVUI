#pragma once
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeVisual.h>
#include <LVUI/TableCellItem.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor table_cell_item_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct TableCellItemInput final {
 TableCellItemConfiguration configuration;std::shared_ptr<const TableCellValueValidator> validator;
 friend bool operator==(const TableCellItemInput&,const TableCellItemInput&)=default;
};
struct NativeTableCellItemCallbacks final {std::function<void(std::string)> edited,submitted;std::function<void(std::string,std::string)> rejected;};
template<class NativeModel,class NativeField>struct TableCellItemHostPolicy final {
 using Model=NativeModel;using Input=TableCellItemInput;using Callbacks=NativeTableCellItemCallbacks;
 using Rect=TableCellItemRect;using Field=NativeField;using InputConfiguration=InputFieldConfiguration;using Validator=TableCellValueValidator;
 [[nodiscard]] static std::string display_text(std::string_view text){return native_display_text(text);}
};
template<template<class>class NativeModel,template<class>class NativeHost,class NativeField>
using TableCellItemBinding=NativeHost<TableCellItemHostPolicy<TableCellItemModelBinding<NativeModel>,NativeField>>;
} // namespace lvui::native
