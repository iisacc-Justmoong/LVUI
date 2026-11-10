#pragma once
#include <LVUI/NativeVirtualCollection.h>

namespace lvui::native {
enum class CollectionSelectionMode {none,single,multiple};
enum class CollectionCommand {previous,next,left,right,up,down,home,end,page_previous,page_next,toggle,select_all,clear,activate};
struct CollectionNavigationOptions final {CollectionSelectionMode mode=CollectionSelectionMode::single;bool select_on_move=true,anchor_on_unselected_focus=false;friend bool operator==(const CollectionNavigationOptions&,const CollectionNavigationOptions&)=default;};
struct CollectionNavigationLayout final {std::size_t columns=1,page_rows=1;bool horizontal=false;std::optional<std::size_t> current_index;};
struct CollectionModifiers final {bool range=false,additive=false;std::optional<std::size_t> anchor_index;};
struct CollectionSelectionState final {
    std::string current_key,anchor_key;
    std::vector<std::string> selected_keys;
    friend bool operator==(const CollectionSelectionState&,const CollectionSelectionState&)=default;
};
struct CollectionNavigationReply final {
    bool handled=false,focus_changed=false,selection_changed=false,activated=false;
    std::optional<std::size_t> index;
};
struct AnyCollectionItem final {bool operator()(const auto&,std::size_t)const{return true;}};
struct CollectionNavigationPolicy final {
    using CollectionSelectionMode=::lvui::native::CollectionSelectionMode;
    using CollectionCommand=::lvui::native::CollectionCommand;
    using CollectionNavigationOptions=::lvui::native::CollectionNavigationOptions;
    using CollectionNavigationLayout=::lvui::native::CollectionNavigationLayout;
    using CollectionModifiers=::lvui::native::CollectionModifiers;
    using CollectionSelectionState=::lvui::native::CollectionSelectionState;
    using CollectionNavigationReply=::lvui::native::CollectionNavigationReply;
    using AnyCollectionItem=::lvui::native::AnyCollectionItem;
    static constexpr std::size_t maximum_items=maximum_collection_items,maximum_key_bytes=maximum_collection_key_bytes;
    [[nodiscard]] static std::string collection_key(std::string_view value){return ::lvui::native::collection_key(value);}
};
template<template<class>class Engine>using CollectionNavigationBinding=Engine<CollectionNavigationPolicy>;

} // namespace lvui::native
