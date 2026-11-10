#pragma once

#include <LVUI/TableAppearance.h>
#include <LVUI/Table.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory>
#include <locale>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace lvui {

class TableCellValueValidator {
public:
    virtual ~TableCellValueValidator()=default;
    [[nodiscard]] virtual TableResult<void> validate(std::string_view text,std::string_view value_type)const=0;
};
struct TableCellItemConfiguration final {
    std::string text="Text",value_type="string",accessibility_name;
    double width=234,height=24,padding=8,font_size=13,current_border_width=1;
    bool enabled=true,visible=true,inputable=false,selected=false,current=false,show_divider=true,clip_content=true;
    TablePalette palette;
    friend bool operator==(const TableCellItemConfiguration&,const TableCellItemConfiguration&)=default;
};
struct TableCellItemRect final {double x=0,y=0,width=0,height=0;};
struct TableCellItemGeometry final {TableCellItemRect surface,text,divider;};
struct TableCellItemVisual final {bool selected=false,current=false;StyleColor text,selection,border,divider;};

// Validation is replaceable and runs on a candidate before the cell publishes it.
struct TableCellItemModelPolicy final {
 using Configuration=TableCellItemConfiguration;using Geometry=TableCellItemGeometry;using Visual=TableCellItemVisual;
 using Color=StyleColor;using Validator=TableCellValueValidator;using Result=TableResult<void>;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
 [[nodiscard]] static Result reject(std::string message){return std::unexpected(TableError{TableErrorCode::invalid,std::move(message)});}
};
template<template<class>class Model>using TableCellItemModelBinding=Model<TableCellItemModelPolicy>;

} // namespace lvui
