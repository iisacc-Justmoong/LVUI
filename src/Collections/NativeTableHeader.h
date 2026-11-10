#pragma once
#include <LVUI/NativeTableHeaderPresentation.h>
#include <LVUI/NativeTableViewGroup.h>
#include <LVUI/NativeVisual.h>
#include <LVUI/NativeInputEvents.h>
#include <LVUI/TableHeader.h>
#include <LVUI/TableText.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace lvui::native {
template<class Color> [[nodiscard]] inline StyleColor table_header_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline std::vector<TableColumn> table_header_columns(std::vector<TableColumn> value){return value;}
[[nodiscard]] inline std::vector<TableColumn> table_header_columns(const std::vector<std::string>& values){
    std::vector<TableColumn> columns;columns.reserve(values.size());
    for(std::size_t i=0;i<values.size();++i)columns.push_back({std::to_string(i),values[i]});return columns;
}
template<std::ranges::input_range Range>
void table_header_widths(std::vector<TableColumn>& columns,const Range& values){
    std::size_t index=0;
    for(const auto value:values){
        if(index>=columns.size())throw std::invalid_argument("More column widths than header columns");
        const auto width=double(value);
        if(!std::isfinite(width)||width<0||width>8192||(width>0&&width<32))throw std::invalid_argument("Invalid header column width");
        columns[index++].width=width;
    }
}
struct TableHeaderInput final {
    TableHeaderConfiguration configuration;
    double offset=0;
    TableText text;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const TableHeaderInput&,const TableHeaderInput&)=default;
};
struct NativeTableHeaderCallbacks final {std::function<void(std::size_t,const TableColumn&)> column_clicked;};

template<class NativeModel,class NativePresentation,class NativeViewGroup,class Builder,class Rect,class Viewport>
struct TableHeaderHostPolicy final {
    using Input=TableHeaderInput;using Model=NativeModel;using Frame=TableViewportFrameValue<Viewport>;
    using Presentation=NativePresentation;using ViewGroup=NativeViewGroup;
    using Views=NativeTableViewsValue<Builder>;using ViewItem=TableViewItemValue<Rect>;using ViewSnapshot=TableViewSnapshot;
    using Callbacks=NativeTableHeaderCallbacks;using SortOrder=TableSortOrder;
    [[nodiscard]] static bool valid_text(const TableText& value){return valid_table_text(value);}
    [[nodiscard]] static TableHeaderPresentation appearance(const TableHeaderConfiguration& value){return table_header_presentation(value);}
    [[nodiscard]] static std::string unnamed_column(const std::string& value,const std::string& number){return lvui::detail::format_table_text(value,{{"column",number}});}
    [[nodiscard]] static std::string display_text(std::string_view value,std::size_t limit){return native_display_text(value,limit);}
    template<class Kind>[[nodiscard]] static bool pointer_canceled(Kind value)noexcept{return lvui::native::pointer_canceled(value);}
};
template<template<class>class Host,class Model,class Presentation,class Group,class Builder,class Rect,class Viewport>
using TableHeaderBinding=Host<TableHeaderHostPolicy<Model,Presentation,Group,Builder,Rect,Viewport>>;
} // namespace lvui::native
