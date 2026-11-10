#pragma once
#include <LVUI/List.h>
#include <LVUI/ListDrag.h>
#include <LVUI/ListItem.h>
#include <LVUI/ListRowContext.h>
#include <LVUI/IconCatalog.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace lvui::native {
template<class Color> [[nodiscard]] inline StyleColor list_view_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}

using ListViewPalette = ListItemPalette;
struct ListViewInput final {
    std::shared_ptr<ListController> controller;
    std::shared_ptr<ListEditor> editor;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t source_revision=0,icon_revision=0,view_revision=0;
    double width=200,height=140,row_height=20,left_padding=4,right_padding=4,icon_size=14,spacing=6,corner_radius=4;
    std::size_t overscan=1;
    bool enabled=true,interactive=true,keyboard_navigation=true,show_indicator=true,editable=false,reorderable=false,scrollable=true;
    ListViewPalette palette;
    friend bool operator==(const ListViewInput&,const ListViewInput&)=default;
};
struct ListViewEvent final { ListEntry entry; std::size_t index=0; };
struct ListEditEvent final { ListEntry entry; std::size_t index=0; std::string text; };
struct NativeListViewCallbacks final {
    std::function<void(const ListViewEvent&)> activated;
    std::function<void(const ListEditEvent&)> edited,submitted;
    std::function<void(const ListEditEvent&,const ListError&)> rejected;
    std::function<void(const ListDragProposal&)> moved;
};
template<class Builder>using ListRowViewFactoryValue=std::function<ii::core::Result<void>(Builder&,std::shared_ptr<const ListRowContext>)>;

} // namespace lvui::native
