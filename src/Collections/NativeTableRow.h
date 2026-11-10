#pragma once
#include <LVUI/NativeTableCellItem.h>
#include <LVUI/TableRowView.h>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor table_row_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline std::vector<TableRowCell> table_row_cells(std::vector<TableRowCell> cells){return cells;}
[[nodiscard]] inline std::vector<TableRowCell> table_row_cells(const std::vector<TableCell>& values){
    std::vector<TableRowCell> cells;cells.reserve(values.size());
    for(std::size_t i=0;i<values.size();++i)cells.push_back({std::to_string(i),values[i]});return cells;
}
[[nodiscard]] inline std::vector<TableRowCell> table_row_cells(const std::vector<std::string>& values){
    std::vector<TableRowCell> cells;cells.reserve(values.size());
    for(std::size_t i=0;i<values.size();++i)cells.push_back({std::to_string(i),TableCell{values[i]}});return cells;
}
struct TableRowInput final {
    TableRowViewConfiguration configuration;
    double offset=0;
    friend bool operator==(const TableRowInput&,const TableRowInput&)=default;
};
struct NativeTableRowCallbacks final {
    std::function<void(std::size_t,std::string)> edited,submitted;
    std::function<void(std::size_t,std::string,std::string)> rejected;
};

template<class NativeModel,class NativeCell>struct TableRowHostPolicy final {
 using Model=NativeModel;using Cell=NativeCell;using Input=TableRowInput;using CellInput=TableCellItemInput;using Callbacks=NativeTableRowCallbacks;
};
template<template<class>class NativeHost,class NativeModel,class NativeCell>
using TableRowBinding=NativeHost<TableRowHostPolicy<NativeModel,NativeCell>>;
} // namespace lvui::native
