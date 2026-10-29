#pragma once
#include "NativeCollection.h"
#include "NativeLineIndex.h"
#include <unordered_map>

namespace lvui::native {
struct VirtualCollectionLayout final {
    double item_height=0,item_width=0,spacing=0;
    bool horizontal=false;
    std::size_t columns=0,overscan=1;
    bool measure_height=false;
    friend bool operator==(const VirtualCollectionLayout&,const VirtualCollectionLayout&)=default;
};
enum class CollectionReveal {contain,begin,end,center};
struct VirtualViewportState final {
    double width=0,height=0,x=0,y=0;
    std::uint64_t movement=0;
    std::string reveal_key;
    CollectionReveal reveal_alignment=CollectionReveal::contain;
    std::uint64_t reveal_serial=0;
    friend bool operator==(const VirtualViewportState&,const VirtualViewportState&)=default;
};
struct VirtualAnchor final {
    std::string key;
    std::size_t index=0;
    double delta=0;
    std::uint64_t movement=0;
    friend bool operator==(const VirtualAnchor&,const VirtualAnchor&)=default;
};
struct VirtualCell final {
    double x=0,y=0,width=0,height=0;
    friend bool operator==(const VirtualCell&,const VirtualCell&)=default;
};
template<class T> class VirtualCollectionSource;
template<class T> struct VirtualCollectionFrame final {
    CollectionFrame<T> collection;
    std::vector<VirtualCell> cells;
    std::size_t columns=1;
    double width=0,height=0,content_width=0,content_height=0,x=0,y=0;
    std::uint64_t movement=0;
    VirtualAnchor anchor;
    bool measure_height=false;
    std::uint64_t measurement_generation=0;
    VirtualCollectionLayout layout;
    std::shared_ptr<const VirtualCollectionSource<T>> source;
    std::string revealed_key;
    std::uint64_t reveal_serial=0;
    friend bool operator==(const VirtualCollectionFrame&,const VirtualCollectionFrame&)=default;
};

// Own one stable random-access model snapshot per source publication. Viewport
// movement reuses its validated keys and index rather than scanning the model.
template<class T> class VirtualCollectionSource final {
public:
    using Item=typename CollectionFrame<T>::Item;
    template<class Range,class Key> requires std::ranges::forward_range<const Range>
    VirtualCollectionSource(const Range& values,Key key){
        if constexpr(std::ranges::sized_range<const Range>){
            if(std::ranges::size(values)>maximum_collection_items)throw std::length_error("Collection item limit is 65536");
            items_.reserve(std::ranges::size(values));
        }
        std::size_t bytes=0;
        for(const auto& value:values){
            if(items_.size()==maximum_collection_items)throw std::length_error("Collection item limit is 65536");
            const auto index=items_.size();auto identity=collection_key(std::invoke(key,value,index));
            if(identity.size()>maximum_collection_key_bytes-bytes)throw std::length_error("Collection keys exceed 16 MiB");
            bytes+=identity.size();
            if(!positions_.emplace(identity,index).second)throw std::invalid_argument("Duplicate collection key");
            items_.push_back({std::move(identity),value_copy(value),index});
        }
    }
    const std::vector<Item>& items()const noexcept{return items_;}
    std::optional<std::size_t> find(const std::string& key)const{
        const auto found=positions_.find(key);return found==positions_.end()?std::nullopt:std::optional(found->second);
    }
private:
    std::vector<Item> items_;
    std::unordered_map<std::string,std::size_t> positions_;
};
template<class Range,class Key> requires std::ranges::forward_range<const Range>
auto virtual_collection_source(const Range& values,Key key){
    return std::make_shared<const VirtualCollectionSource<collection_item_t<Range>>>(values,std::move(key));
}

// Pure native planning copies only visible/overscan cells from a validated
// snapshot. No GUI dependency or document mutation is involved.
template<class T>
auto virtual_collection_frame(const VirtualCollectionSource<T>& source,VirtualCollectionLayout layout,
                              VirtualViewportState viewport,VirtualAnchor anchor={},const VirtualLineIndex* measured=nullptr){
    const auto metric=[](double n){return std::isfinite(n)&&n>=0&&n<=32767;};
    if(!metric(layout.item_height)||!layout.item_height||!metric(layout.item_width)||!metric(layout.spacing)||
       layout.columns>maximum_collection_items||layout.overscan>maximum_collection_items||
       (layout.horizontal&&(!layout.item_width||layout.columns>1)))
        throw std::invalid_argument("Invalid virtual collection layout");
    if(!metric(viewport.width)||!metric(viewport.height)||viewport.width>8192||viewport.height>8192||
       !std::isfinite(viewport.x)||!std::isfinite(viewport.y)||viewport.x<0||viewport.y<0||
       !std::isfinite(anchor.delta)||anchor.delta<0||static_cast<unsigned>(viewport.reveal_alignment)>3)
        throw std::invalid_argument("Invalid virtual collection viewport or anchor");
    const auto& items=source.items();
    VirtualCollectionFrame<T> result;
    result.width=viewport.width;result.height=viewport.height;
    result.movement=viewport.movement;
    result.layout=layout;result.reveal_serial=viewport.reveal_serial;
    if(!layout.horizontal){
        result.columns=layout.columns?layout.columns:layout.item_width?
            std::size_t(std::clamp(std::floor((viewport.width+layout.spacing)/(layout.item_width+layout.spacing)),1.,double(maximum_collection_items))):1;
    }
    const auto cell_width=layout.horizontal?layout.item_width:
        std::max(0.,(viewport.width-layout.spacing*double(result.columns-1))/double(result.columns));
    const auto line_count=layout.horizontal?items.size():(items.size()+result.columns-1)/result.columns;
    if(layout.measure_height&&(layout.horizontal||!measured||measured->size()!=line_count))
        throw std::invalid_argument("Measured heights require a vertical native line index");
    result.measure_height=layout.measure_height;
    const auto stride=(layout.horizontal?layout.item_width:layout.item_height)+layout.spacing;
    const auto logical=layout.measure_height?measured->extent():line_count?double(line_count)*stride-layout.spacing:0.;
    const auto start=[&](std::size_t line){return layout.measure_height?measured->start(line):double(line)*stride;};
    const auto height=[&](std::size_t line){return layout.measure_height?measured->height(line):layout.item_height;};
    result.content_width=layout.horizontal?logical:viewport.width;
    result.content_height=layout.horizontal?viewport.height:logical;
    auto offset=layout.horizontal?viewport.x:viewport.y;
    std::optional<std::size_t> anchored_index;
    if(!anchor.key.empty()&&anchor.movement==viewport.movement&&!items.empty()){
        const auto index=source.find(anchor.key).value_or(std::min(anchor.index,items.size()-1));
        anchored_index=index;
        const auto line=layout.horizontal?index:index/result.columns;
        const auto length=layout.measure_height?height(line)+layout.spacing:stride;
        offset=start(line)+std::min(anchor.delta,std::max(0.,std::nextafter(length,0.)));
    }
    const auto visible_extent=layout.horizontal?viewport.width:viewport.height;
    if(viewport.width>0&&viewport.height>0&&!viewport.reveal_key.empty())if(auto index=source.find(viewport.reveal_key)){
        const auto line=layout.horizontal?*index:*index/result.columns;
        const auto beginning=start(line),length=layout.horizontal?layout.item_width:height(line);
        if(viewport.reveal_alignment==CollectionReveal::begin||length>=visible_extent)offset=beginning;
        else if(viewport.reveal_alignment==CollectionReveal::end)offset=beginning+length-visible_extent;
        else if(viewport.reveal_alignment==CollectionReveal::center)offset=beginning+(length-visible_extent)/2;
        else if(beginning<offset)offset=beginning;
        else if(beginning+length>offset+visible_extent)offset=beginning+length-visible_extent;
        result.revealed_key=viewport.reveal_key;
    }
    offset=std::clamp(offset,0.,std::max(0.,logical-visible_extent));
    result.x=layout.horizontal?offset:0;result.y=layout.horizontal?0:offset;
    std::size_t first=0,count=0;
    if(viewport.width>0&&viewport.height>0&&!items.empty()){
        const auto leading=layout.measure_height?measured->at(offset):std::size_t(std::clamp(std::floor(offset/stride),0.,double(line_count-1)));
        const auto last=layout.measure_height?(logical==0?line_count:measured->end(std::min(offset+visible_extent,logical))):std::size_t(std::clamp(std::ceil((offset+visible_extent)/stride),0.,double(line_count)));
        const auto begin=leading>layout.overscan?leading-layout.overscan:0;
        const auto end=std::min(line_count,last+layout.overscan);
        const auto per_line=layout.horizontal?1:result.columns;
        first=begin*per_line;count=std::min(items.size(),end*per_line)-first;
        const auto index=anchored_index&&*anchored_index/per_line==leading?*anchored_index:leading*per_line;
        result.anchor={items[index].key,index,offset-start(leading),viewport.movement};
        if(logical==0)result.anchor={}; // An invisible probe is not a review anchor.
        if(layout.measure_height&&logical==0){first=items.size()-1;count=1;}
    }
    result.collection.total=items.size();
    result.collection.options={.first=first,.count=count,.spacing=layout.spacing,.horizontal=layout.horizontal,.sparse=layout.measure_height};
    if(layout.measure_height){
        for(std::size_t n=first;n<first+count;++n)if(logical==0||height(n/result.columns)>0)result.collection.items.push_back(items[n]);
        result.collection.options.first=result.collection.items.empty()?first:result.collection.items.front().index;
        result.collection.options.count=result.collection.items.size();
    }else result.collection.items.assign(items.begin()+first,items.begin()+first+count);
    result.cells.reserve(result.collection.items.size());
    for(const auto& item:result.collection.items){
        result.cells.push_back(layout.horizontal?
            VirtualCell{double(item.index)*stride-offset,0,cell_width,viewport.height}:
            VirtualCell{double(item.index%result.columns)*(cell_width+layout.spacing),start(item.index/result.columns)-offset,cell_width,height(item.index/result.columns)});
    }
    return result;
}
template<class Range,class Key> requires std::ranges::forward_range<const Range>
auto virtual_collection_frame(const Range& values,Key key,VirtualCollectionLayout layout,
                              VirtualViewportState viewport,VirtualAnchor anchor={}){
    return virtual_collection_frame(*virtual_collection_source(values,std::move(key)),layout,viewport,std::move(anchor));
}

// View-only owner-thread state. Movement distinguishes user scrolling from a
// programmatic anchor correction or a resize, preventing stale anchor feedback.
class VirtualViewport final {
public:
    VirtualViewport()=default;
    VirtualViewport(const VirtualViewport&)=delete;
    VirtualViewport& operator=(const VirtualViewport&)=delete;
    VirtualViewportState read(ReadContext& context){owner();return context.read(state_);}
    VirtualViewportState current()const{owner();return state_.get();}
    VirtualAnchor anchor()const{owner();return anchor_;}
    void reveal(std::string key,CollectionReveal alignment=CollectionReveal::contain){
        owner();if(!active_)return;
        if(static_cast<unsigned>(alignment)>3)throw std::invalid_argument("Invalid collection reveal alignment");
        if(!key.empty())key=collection_key(key);
        auto next=state_.get();if(next.reveal_key==key&&next.reveal_alignment==alignment)return;
        if(next.movement==UINT64_MAX||next.reveal_serial==UINT64_MAX)throw std::overflow_error("Viewport reveal exhausted");
        next.reveal_key=std::move(key);next.reveal_alignment=alignment;++next.reveal_serial;++next.movement;state_.set(std::move(next));
    }
    void observe(VirtualViewportState value){
        owner();if(!active_)return;
        if(!std::isfinite(value.width)||!std::isfinite(value.height)||value.width<0||value.height<0||value.width>8192||value.height>8192||
           !std::isfinite(value.x)||!std::isfinite(value.y)||value.x<0||value.y<0)
            throw std::invalid_argument("Invalid observed viewport");
        const auto before=state_.get();
        value.movement=before.movement;
        value.reveal_key=before.reveal_key;value.reveal_alignment=before.reveal_alignment;value.reveal_serial=before.reveal_serial;
        if(value.x!=before.x||value.y!=before.y){
            if(value.movement==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Viewport movement exhausted");
            ++value.movement;
            value.reveal_key.clear();
        }
        state_.set(value);
    }
    template<class T> bool commit(const VirtualCollectionFrame<T>& frame){
        owner();if(!active_)return false;
        if(!std::isfinite(frame.x)||!std::isfinite(frame.y)||frame.x<0||frame.y<0||
           !std::isfinite(frame.anchor.delta)||frame.anchor.delta<0)
            throw std::invalid_argument("Invalid viewport commit");
        auto current=state_.get();
        if(frame.movement!=current.movement||frame.width!=current.width||frame.height!=current.height)return false;
        anchor_=frame.anchor;
        current.x=frame.x;current.y=frame.y;
        if(current.reveal_serial==frame.reveal_serial&&current.reveal_key==frame.revealed_key)current.reveal_key.clear();
        state_.set(current);return true;
    }
    void close(){owner();active_=false;anchor_={};}
private:
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Viewport requires its owner thread");}
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true;
    ii::core::Observable<VirtualViewportState> state_{VirtualViewportState{}};
    VirtualAnchor anchor_;
};
} // namespace lvui::native
