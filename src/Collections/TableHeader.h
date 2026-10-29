#pragma once

#include <LVUI/TableAppearance.h>
#include <LVUI/TableViewport.h>

#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace lvui {

struct TableHeaderConfiguration final {
    std::vector<TableColumn> columns{{"0","Column"},{"1","Column"},{"2","Column"}};
    double width=717,row_height=24,horizontal_padding=8,separator_height=1;
    double minimum_column_width=32,fallback_column_width=0;
    bool fit_flexible_columns=true,interactive=false,enabled=true,visible=true;
    TablePalette palette;
    friend bool operator==(const TableHeaderConfiguration&,const TableHeaderConfiguration&)=default;
};

// Column widths and clipping use the same independent viewport model as Table.
class TableHeader final {
public:
    TableHeader(){(void)set_configuration(configuration_);}
    [[nodiscard]] static bool validate(const TableHeaderConfiguration& value) {
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        const auto& p=value.palette;
        if(!bound(value.width,1,8192)||!bound(value.row_height,1,8191)||!bound(value.horizontal_padding,0,8192)||
           !bound(value.separator_height,0,8192-value.row_height)||!bound(value.minimum_column_width,32,8192)||
           !bound(value.fallback_column_width,0,8192)||(value.fallback_column_width>0&&value.fallback_column_width<value.minimum_column_width)||
           !color(p.background)||!color(p.divider)||!color(p.header_text)||!color(p.cell_text)||!color(p.selection)||!color(p.current))return false;
        auto snapshot=std::make_shared<TableSnapshot>();snapshot->columns=value.columns;
        return bool(TableIndex::create(std::move(snapshot)));
    }
    bool set_configuration(TableHeaderConfiguration value) {
        if(!validate(value))return false;
        auto snapshot=std::make_shared<TableSnapshot>();snapshot->columns=value.columns;
        configuration_=std::move(value);snapshot_=std::move(snapshot);return true;
    }
    [[nodiscard]] const TableHeaderConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] TableResult<TableViewportFrame> layout(double width,double offset)const {
        if(!std::isfinite(width)||width<1||width>8192||!std::isfinite(offset)||offset<0||offset>1e15)
            return std::unexpected(TableError{TableErrorCode::invalid,"Invalid table header viewport"});
        auto source=TableViewportSource::create(snapshot_,{.width=width,
            .fallback_column_width=configuration_.fallback_column_width?configuration_.fallback_column_width:configuration_.minimum_column_width,
            .minimum_column_width=configuration_.minimum_column_width,.fit_flexible_columns=configuration_.fit_flexible_columns});
        if(!source)return std::unexpected(source.error());
        return (*source)->frame({.height=configuration_.row_height,.header_height=0,.x=offset,.overscan_rows=0,.overscan_columns=0});
    }
    [[nodiscard]] std::optional<std::size_t> hit(const TableViewportFrame& frame,double x)const noexcept {
        if(!std::isfinite(x)||x<0||x>=frame.width)return std::nullopt;
        for(const auto& column:frame.columns)if(x>=column.offset&&x<column.offset+column.extent){
            if(column.index<configuration_.columns.size()&&configuration_.columns[column.index].enabled)return column.index;
            return std::nullopt;
        }
        return std::nullopt;
    }
private:
    TableHeaderConfiguration configuration_;
    std::shared_ptr<const TableSnapshot> snapshot_;
};

} // namespace lvui
