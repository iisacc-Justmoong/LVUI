#pragma once

#include "List.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

enum class ListReveal { contain, start, end, center };
struct ListViewportAnchor final { std::string key; std::size_t index=0; double within=0; std::uint64_t movement=0; };
struct ListViewportRequest final {
    double width=0,height=0,offset=0,row_height=20;
    std::size_t overscan=1;
    std::uint64_t movement=0;
    ListViewportAnchor anchor;
    std::string reveal_key;
    ListReveal reveal=ListReveal::contain;
};
struct ListViewportRow final { std::size_t index=0; double y=0,height=0; };
struct ListViewportFrame final {
    std::uint64_t source_revision=0;
    double content_height=0,offset=0;
    std::vector<ListViewportRow> rows;
    ListViewportAnchor anchor;
    std::string revealed_key;
};

// A viewport holds an immutable index; an application may keep a frame and
// its source snapshot while a newer source is being prepared elsewhere.
class ListViewportSource final {
public:
    [[nodiscard]] static ListResult<ListViewportSource> create(std::shared_ptr<const ListSnapshot> snapshot){
        auto index=ListIndex::create(std::move(snapshot));if(!index)return std::unexpected(index.error());
        return ListViewportSource(std::move(*index));
    }
    [[nodiscard]] std::shared_ptr<const ListSnapshot> snapshot()const{return index_.snapshot();}
    [[nodiscard]] ListResult<ListViewportFrame> frame(const ListViewportRequest& request)const{
        const auto extent=[](double value){return std::isfinite(value)&&value>=0&&value<=8192;};
        if(!extent(request.width)||!extent(request.height)||!std::isfinite(request.offset)||request.offset<0||
           !std::isfinite(request.row_height)||request.row_height<18||request.row_height>256||request.overscan>128||
           !std::isfinite(request.anchor.within)||request.anchor.within<0||unsigned(request.reveal)>unsigned(ListReveal::center))
            return error(ListErrorCode::invalid,"Invalid list viewport request");
        const auto data=index_.snapshot();const auto count=data->entries.size();
        ListViewportFrame result{.source_revision=data->revision,.content_height=double(count)*request.row_height};
        auto offset=request.offset;
        if(!request.anchor.key.empty()&&request.anchor.movement==request.movement){
            if(auto index=index_.find(request.anchor.key)){
                const auto begin=double(*index)*request.row_height,end=begin+request.row_height;
                offset=std::min(begin+request.anchor.within,std::nextafter(end,begin));
            }
        }
        if(!request.reveal_key.empty()){
            auto found=index_.find(request.reveal_key);if(!found)return error(ListErrorCode::not_found,"List reveal key was not found");
            const auto begin=double(*found)*request.row_height,end=begin+request.row_height;
            if(request.reveal==ListReveal::start)offset=begin;
            else if(request.reveal==ListReveal::end)offset=end-request.height;
            else if(request.reveal==ListReveal::center)offset=begin+(request.row_height-request.height)/2;
            else if(begin<offset)offset=begin;
            else if(end>offset+request.height)offset=end-request.height;
            result.revealed_key=request.reveal_key;
        }
        offset=std::clamp(offset,0.,std::max(0.,result.content_height-request.height));result.offset=offset;
        if(!count||request.width==0||request.height==0)return result;
        const auto first_visible=std::min(count-1,std::size_t(std::floor(offset/request.row_height)));
        const auto visible_end=std::min(count,std::size_t(std::ceil((offset+request.height)/request.row_height)));
        const auto first=first_visible>request.overscan?first_visible-request.overscan:0;
        const auto last=std::min(count,visible_end+request.overscan);result.rows.reserve(last-first);
        for(auto index=first;index<last;++index)result.rows.push_back({index,double(index)*request.row_height-offset,request.row_height});
        result.anchor={data->entries[first_visible].key,first_visible,offset-double(first_visible)*request.row_height,request.movement};
        return result;
    }
private:
    explicit ListViewportSource(ListIndex index):index_(std::move(index)){}
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    ListIndex index_;
};

} // namespace lvui
