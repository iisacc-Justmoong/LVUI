#pragma once
#include <LVUI/NativeCollection.h>
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
    using Value=T;
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

// View contracts are bound by the application to the iiGUI planner.
struct VirtualCollectionPlannerPolicy final {
    using Layout=VirtualCollectionLayout;
    using Viewport=VirtualViewportState;
    using Anchor=VirtualAnchor;
    using Cell=VirtualCell;
    using Reveal=CollectionReveal;
    template<class T> using Frame=VirtualCollectionFrame<T>;
    static constexpr std::size_t maximum_items=maximum_collection_items;
};
template<template<class>class Engine>using VirtualCollectionPlannerBinding=Engine<VirtualCollectionPlannerPolicy>;

// View-only owner-thread state. Movement distinguishes user scrolling from a
// programmatic anchor correction or a resize, preventing stale anchor feedback.
struct VirtualViewportPolicy final {
    using VirtualViewportState=::lvui::native::VirtualViewportState;using VirtualAnchor=::lvui::native::VirtualAnchor;using CollectionReveal=::lvui::native::CollectionReveal;
    [[nodiscard]] static std::string collection_key(std::string_view value){return ::lvui::native::collection_key(value);}
};
template<template<class>class Engine>using VirtualViewportBinding=Engine<VirtualViewportPolicy>;

} // namespace lvui::native
