#pragma once

#include <LVUI/TableAppearance.h>
#include <LVUI/TableSorting.h>
#include <LVUI/TableViewport.h>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lvui {

// The application owns sorting; this is only its current presentation state.
struct TableHeaderSort final {
    std::string column;
    TableSortOrder order=TableSortOrder::ascending;
    friend bool operator==(const TableHeaderSort&,const TableHeaderSort&)=default;
};
// Unset per-column values inherit the shared defaults; empty icon names hide a slot.
struct TableHeaderColumnAppearance final {
    std::string column;
    std::optional<double> padding;
    std::optional<std::string> leading_icon,trailing_icon;
    friend bool operator==(const TableHeaderColumnAppearance&,const TableHeaderColumnAppearance&)=default;
};
// Keyed presentation is independent of a table's data and survives column removal.
struct TableHeaderPresentation final {
    std::string default_leading_icon="generalchevronDown",default_trailing_icon;
    double icon_size=18,icon_spacing=7;
    std::vector<TableHeaderColumnAppearance> column_appearance;
    friend bool operator==(const TableHeaderPresentation&,const TableHeaderPresentation&)=default;
};
[[nodiscard]] inline bool valid_table_header_presentation(const TableHeaderPresentation& value){
    const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
    const auto icon=[](const std::string& name){return name.empty()||(name.size()<=256&&
        name.find_first_not_of(" \t\r\n")!=name.npos&&valid_accessibility_text(name));};
    if(!bound(value.icon_size,0,256)||(value.icon_size>0&&value.icon_size<1)||!bound(value.icon_spacing,0,8192)||
       !icon(value.default_leading_icon)||!icon(value.default_trailing_icon)||value.column_appearance.size()>table_maximum_columns)return false;
    std::unordered_set<std::string_view> styled;styled.reserve(value.column_appearance.size());
    for(const auto& item:value.column_appearance)
        if(item.column.empty()||item.column.size()>1024||!valid_accessibility_text(item.column)||!styled.emplace(item.column).second||
           (item.padding&&!bound(*item.padding,0,8192))||(item.leading_icon&&!icon(*item.leading_icon))||
           (item.trailing_icon&&!icon(*item.trailing_icon)))return false;
    return true;
}
struct TableHeaderConfiguration final {
    std::vector<TableColumn> columns{{"0","Column"},{"1","Column"},{"2","Column"}};
    double width=717,row_height=24,horizontal_padding=8,separator_height=1;
    double minimum_column_width=32,fallback_column_width=0;
    bool fit_flexible_columns=true,interactive=false,enabled=true,visible=true;
    TablePalette palette;
    std::optional<TableHeaderSort> sort;
    std::string default_leading_icon="generalchevronDown",default_trailing_icon;
    double icon_size=18,icon_spacing=7;
    std::vector<TableHeaderColumnAppearance> column_appearance;
    friend bool operator==(const TableHeaderConfiguration&,const TableHeaderConfiguration&)=default;
};
[[nodiscard]] inline TableHeaderPresentation table_header_presentation(const TableHeaderConfiguration& value){
    return {value.default_leading_icon,value.default_trailing_icon,value.icon_size,value.icon_spacing,value.column_appearance};
}

// The application supplies the table data and viewport contracts to iiGUI.
template<class Viewport>struct TableHeaderModelPolicy final {
    using Configuration=TableHeaderConfiguration;using Color=StyleColor;using Snapshot=TableSnapshot;
    using Index=TableIndex;using ViewportSource=Viewport;using ViewportConfiguration=TableViewportConfiguration;
    using ViewportRequest=TableViewportRequest;using ViewportFrame=TableViewportFrameValue<Viewport>;
    using Error=TableError;using ErrorCode=TableErrorCode;using SortOrder=TableSortOrder;
    template<class T>using Result=TableResult<T>;
    [[nodiscard]] static bool valid_presentation(const Configuration& value){return valid_table_header_presentation(table_header_presentation(value));}
};
template<template<class>class Model,class Viewport>using TableHeaderModelBinding=Model<TableHeaderModelPolicy<Viewport>>;

} // namespace lvui
