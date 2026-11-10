#pragma once

#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeListItemDrawing.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
template<class Color>
[[nodiscard]] inline StyleColor list_item_color(const Color& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
[[nodiscard]] inline ListItemVariant list_item_variant(ListItemVariant value) noexcept { return value; }
template<class Image>
struct ListItemInputValue final {
    ListItemConfiguration configuration;
    std::shared_ptr<const Image> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    bool custom_content = false;
    std::optional<std::string> accessibility_description;
    friend bool operator==(const ListItemInputValue&, const ListItemInputValue&) = default;
};
struct NativeListItemCallbacks final { std::function<void()> activated; };

// The application supplies LVUI values and icon assets; iiGUI owns the row.
template<class NativeModel,class NativeInput,class NativeDrawing,class Decoder,class GraphicsPort>
struct ListItemHostPolicy final {
    using Model=NativeModel;using Input=NativeInput;using Drawing=NativeDrawing;using IconCache=NativeIconCache<Decoder>;
    using Callbacks=NativeListItemCallbacks;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static std::string display_text(std::string_view value){return native_display_text(value);}
    [[nodiscard]] static auto resolve_icon(IconCache& cache,const Input& input,const ListItemConfiguration& c) {
        return cache.resolve(c.icon_name,input.icon_source,c.icon_size);
    }
    [[nodiscard]] static auto resolve_chevron(IconCache& cache,const Input&,const ListItemConfiguration& c) {
        return cache.resolve("generalchevronRight",{},c.chevron_size);
    }
    [[nodiscard]] static auto action(const Drawing& drawing){return GraphicsPort::action(drawing);}
    template<class PaintState>[[nodiscard]] static auto paint(const Drawing& drawing,const PaintState& state,bool content) {
        return GraphicsPort::paint(drawing,state,content);
    }
};
template<template<class> class Model,template<class> class Drawing,template<class> class Host,
         class Decoder,class Converter,class GraphicsPort>
using ListItemBinding=Host<ListItemHostPolicy<
    ListItemModelBinding<Model>,ListItemInputValue<typename Decoder::Image>,
    ListItemDrawingBinding<Drawing,ListItemModelBinding<Model>,Converter>,Decoder,GraphicsPort>>;

} // namespace lvui::native
