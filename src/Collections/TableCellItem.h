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
class TableCellItem final {
public:
    [[nodiscard]] static bool validate(const TableCellItemConfiguration& c)noexcept {
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor v){return range(v.red,0,1)&&range(v.green,0,1)&&range(v.blue,0,1)&&range(v.alpha,0,1);};
        const auto& p=c.palette;
        return valid_accessibility_text(c.text)&&valid_accessibility_text(c.accessibility_name)&&!c.value_type.empty()&&
            c.value_type.size()<=128&&valid_accessibility_text(c.value_type)&&range(c.width,1,8192)&&range(c.height,1,8192)&&
            range(c.padding,0,8192)&&range(c.font_size,1,256)&&range(c.current_border_width,0,8192)&&
            color(p.background)&&color(p.divider)&&color(p.header_text)&&color(p.cell_text)&&color(p.selection)&&color(p.current);
    }
    bool set_configuration(TableCellItemConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] const TableCellItemConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<TableCellItemGeometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        TableCellItemGeometry result;result.surface={0,0,width,height};const auto padding=std::min(configuration_.padding,width/2);
        result.text={padding,0,std::max(0.,width-2*padding),height};
        if(configuration_.show_divider&&width>0&&height>0){const auto edge=std::min(1.,width);result.divider={0,0,edge,height};}
        return result;
    }
    [[nodiscard]] TableCellItemVisual visual()const noexcept{
        const auto& c=configuration_;return {c.selected,c.current,c.palette.cell_text,c.palette.selection,c.palette.current,c.palette.divider};
    }
    [[nodiscard]] TableResult<void> accept(std::string_view text,std::shared_ptr<const TableCellValueValidator> validator={})const{
        const auto reject=[](std::string message)->TableResult<void>{return std::unexpected(TableError{TableErrorCode::invalid,std::move(message)});};
        if(!valid_accessibility_text(text))return reject("Invalid table cell text");
        if(validator)try{return validator->validate(text,configuration_.value_type);}catch(...){return reject("Table cell validator failed");}
        const auto& type=configuration_.value_type;
        if(type=="text"||type=="string")return {};
        if(type=="bool"||type=="boolean")return text=="true"||text=="false"?TableResult<void>{}:reject("Expected true or false");
        if(type=="int"||type=="integer"){
            std::int64_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
            return !text.empty()&&error==std::errc{}&&end==text.data()+text.size()?TableResult<void>{}:reject("Expected a signed integer");
        }
        if(type=="number"||type=="double"||type=="float"){
            std::istringstream stream{std::string(text)};stream.imbue(std::locale::classic());double value=0;
            stream >> std::noskipws >> value;
            const bool complete=bool(stream)&&stream.rdbuf()->sgetc()==std::char_traits<char>::eof();
            return !text.empty()&&complete&&std::isfinite(value)?TableResult<void>{}:reject("Expected a finite number");
        }
        return reject("A custom table cell type requires a validator");
    }
private:
    TableCellItemConfiguration configuration_;
};

} // namespace lvui
