#pragma once
#include <LVUI/TextInput.h>
#include <iiGUI/TextBoundary.h>
#include <iiGUI/TextLine.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui::native {
struct NativeTextCaret final { double x=0,y=0,height=0; };
struct NativeTextSelectionRect final { double x=0,y=0,width=0,height=0; };
struct NativeTextLayoutLine final {
    std::size_t first=0,last=0,next=0;
    double y=0,width=0,height=0;
};

// LVUI owns paragraph layout and selection geometry. The host's generic line
// shaper supplies glyph metrics, bidirectional spans, hit testing and raster.
class NativeTextLayout final {
    ii::gui::TextIndex source_;
    std::vector<NativeTextLayoutLine> lines_;
    std::vector<ii::gui::TextLine> shaped_;
    double width_=0,height_=0;
    [[nodiscard]] std::size_t line_for(std::size_t unit) const noexcept {
        const auto it=std::upper_bound(lines_.begin(),lines_.end(),unit,
            [](std::size_t value,const auto& line){return value<line.first;});
        return it==lines_.begin()?0:std::size_t(std::prev(it)-lines_.begin());
    }
public:
    static constexpr std::size_t maximum_lines=4096;
    [[nodiscard]] static ii::core::Result<NativeTextLayout> create(std::string text,
            ii::gui::TextLineStyle style={},bool password=false) {
        if(text.size()>TextInput::maximum_bytes||!valid_accessibility_text(text))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"LVUI text layout requires bounded NUL-free UTF-8");
        std::size_t paragraphs=1;
        for(std::size_t i=0;i<text.size();++i)if(text[i]=='\r'||text[i]=='\n'){
            if(++paragraphs>maximum_lines)return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI text layout exceeds 4096 paragraphs");
            if(text[i]=='\r'&&i+1<text.size()&&text[i+1]=='\n')++i;
        }
        try {
            auto source=ii::gui::TextIndex::create(std::move(text));if(!source)return std::unexpected(source.error());
            NativeTextLayout result;result.source_=std::move(*source);
            const auto input=result.source_.text();
            std::size_t start_byte=0;
            while(true){
                if(result.lines_.size()==maximum_lines)
                    return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI text layout exceeds 4096 paragraphs");
                const auto found=input.find_first_of("\r\n",start_byte);
                const auto end_byte=found==input.npos?input.size():found;
                auto next_byte=end_byte;
                if(found!=input.npos){++next_byte;if(input[found]=='\r'&&next_byte<input.size()&&input[next_byte]=='\n')++next_byte;}
                const auto first=result.source_.utf16_for_byte(start_byte),last=result.source_.utf16_for_byte(end_byte),next=result.source_.utf16_for_byte(next_byte);
                if(!first||!last||!next)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"LVUI paragraph splits a Unicode scalar");
                std::string display;
                if(password){display.reserve((*last-*first)*3);for(auto i=*first;i<*last;++i)display.append("\xe2\x80\xa2");}
                else display.assign(input.substr(start_byte,end_byte-start_byte));
                auto line_index=ii::gui::TextIndex::create(std::move(display));if(!line_index)return std::unexpected(line_index.error());
                auto shaped=ii::gui::TextLine::create(std::move(*line_index),style);if(!shaped)return std::unexpected(shaped.error());
                auto metrics=shaped->metrics();if(!metrics)return std::unexpected(metrics.error());
                result.lines_.push_back({*first,*last,*next,result.height_,metrics->width,metrics->height});
                result.width_=std::max(result.width_,metrics->width);result.height_+=metrics->height;
                result.shaped_.push_back(std::move(*shaped));
                if(found==input.npos)break;
                start_byte=next_byte;
            }
            return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI text layout failed",std::current_exception());}
    }
    [[nodiscard]] std::size_t length()const noexcept{return source_.utf16_size();}
    [[nodiscard]] double width()const noexcept{return width_;}
    [[nodiscard]] double height()const noexcept{return height_;}
    [[nodiscard]] const std::vector<NativeTextLayoutLine>& lines()const noexcept{return lines_;}
    [[nodiscard]] std::string_view display_text(std::size_t line)const noexcept {
        return line<shaped_.size()?shaped_[line].index().text():std::string_view{};
    }
    [[nodiscard]] ii::core::Result<NativeTextCaret> caret(std::size_t unit)const {
        if(!source_.byte_for_utf16(unit))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Caret splits a Unicode scalar");
        const auto i=line_for(unit);const auto& line=lines_[i];
        auto x=shaped_[i].caret(std::min(unit,line.last)-line.first);if(!x)return std::unexpected(x.error());
        return NativeTextCaret{*x,line.y,line.height};
    }
    [[nodiscard]] ii::core::Result<std::size_t> hit_test(double x,double y)const {
        if(!std::isfinite(x)||!std::isfinite(y))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Text hit position must be finite");
        const auto it=std::upper_bound(lines_.begin(),lines_.end(),y,
            [](double value,const auto& line){return value<line.y;});
        const auto i=it==lines_.begin()?std::size_t(0):std::size_t(std::prev(it)-lines_.begin());
        const auto& line=lines_[i];auto hit=shaped_[i].hit_test(x);if(!hit)return std::unexpected(hit.error());
        auto unit=line.first+std::min(*hit,line.last-line.first);
        // Masked text uses one bullet per UTF-16 unit. Snap an interior hit
        // back to an original grapheme edge, including surrogate pairs.
        if(unit>line.first&&unit<line.last){
            const auto before=source_.previous_grapheme(unit),after=source_.next_grapheme(before);
            if(after!=unit){
                auto a=shaped_[i].caret(before-line.first),b=shaped_[i].caret(after-line.first);
                if(!a)return std::unexpected(a.error());if(!b)return std::unexpected(b.error());
                unit=std::abs(*a-x)<=std::abs(*b-x)?before:after;
            }
        }
        return unit;
    }
    [[nodiscard]] ii::core::Result<std::vector<NativeTextSelectionRect>> selection(TextInputRange range)const {
        if(range.location>length()||range.length>length()-range.location||
           !source_.byte_for_utf16(range.location)||!source_.byte_for_utf16(range.location+range.length))
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Selection splits a Unicode scalar");
        try {
            std::vector<NativeTextSelectionRect> result;const auto end=range.location+range.length;
            for(std::size_t i=0;i<lines_.size();++i){
                const auto& line=lines_[i];const auto first=std::max(range.location,line.first),last=std::min(end,line.last);
                if(first<last){auto spans=shaped_[i].selection(first-line.first,last-line.first);if(!spans)return std::unexpected(spans.error());
                    for(auto span:*spans)if(span.width>0)result.push_back({span.x,line.y,span.width,line.height});}
                if(line.next>line.last&&range.location<line.next&&end>line.last){
                    auto edge=shaped_[i].caret(line.last-line.first);if(!edge)return std::unexpected(edge.error());
                    result.push_back({*edge,line.y,3,line.height});
                }
            }
            return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI text selection layout failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<ii::gui::TextBitmap> render_line(std::size_t line,double offset,double width,
            ii::gui::Color color,double scale=1)const {
        if(line>=shaped_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Text paragraph does not exist");
        return shaped_[line].render(offset,width,color,scale);
    }
};
} // namespace lvui::native
