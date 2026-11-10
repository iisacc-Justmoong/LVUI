#pragma once

#include <LVUI/TableCellItem.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lvui {

struct TableRowCell final {
    std::string key;
    TableCell value;
    std::shared_ptr<const TableCellValueValidator> validator;
    bool selected=false,current=false;
    friend bool operator==(const TableRowCell&,const TableRowCell&)=default;
};
struct TableRowViewConfiguration final {
    std::vector<TableRowCell> cells{{"0",{"Text"}},{"1",{"Text"}},{"2",{"Text"}}};
    double width=717,cell_width=234,cell_height=24,content_spacing=8,font_size=13;
    bool inputable=false,enabled=true,visible=true,show_divider=true;
    TablePalette palette;
    friend bool operator==(const TableRowViewConfiguration&,const TableRowViewConfiguration&)=default;
};
struct TableRowSlot final {std::size_t index=0;double x=0,width=0;};
struct TableRowFrame final {std::vector<TableRowSlot> cells;double width=0,height=0,spacing=0,content_width=0,offset=0;};

// Row identity belongs to the caller; a cell key survives moves and retained views.
template<class NativeCell>struct TableRowViewModelPolicy final {
 using Configuration=TableRowViewConfiguration;using CellConfiguration=TableCellItemConfiguration;
 using CellModel=NativeCell;using Frame=TableRowFrame;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Model,class NativeCell>using TableRowViewModelBinding=Model<TableRowViewModelPolicy<NativeCell>>;

} // namespace lvui
