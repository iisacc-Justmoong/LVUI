#pragma once
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeInputEvents.h>
#include <LVUI/NativeMenu.h>
#include <LVUI/NativeTableHeaderPresentation.h>
#include <LVUI/NativeTableViewGroup.h>
#include <LVUI/TableAppearance.h>
#include <LVUI/TableClipboard.h>
#include <LVUI/TableEditing.h>
#include <LVUI/TableMerge.h>
#include <LVUI/TableSorting.h>
#include <LVUI/TableStructure.h>
#include <LVUI/TableText.h>
#include <LVUI/TableViewport.h>
#include <initializer_list>
#include <string_view>
#include <utility>
namespace lvui::native {
template<class Color> [[nodiscard]] inline StyleColor table_color(const Color& value) noexcept {return {value.red,value.green,value.blue,value.alpha};}
struct TableInput final {
    std::shared_ptr<TableController> controller;
    std::shared_ptr<TableEditor> editor;
    std::shared_ptr<TableOrderEditor> order_editor;
    std::shared_ptr<TableStructureEditor> structure_editor;
    std::shared_ptr<const TableSorter> sorter;
    TableAppearance appearance;
    TableText text;
    std::int64_t source_revision = 0;
    bool editable = false, clipboard_enabled = true, sorting_enabled = false;
    bool column_resize_enabled = false, row_resize_enabled = false;
    double column_resize_handle = 6, row_resize_handle = 6;
    std::shared_ptr<TableMergeEditor> merge_editor;
    std::shared_ptr<const TableStructureFactory> structure_factory;
    bool structure_controls_visible = false, add_row_controls_visible = true, add_column_controls_visible = true;
    bool delete_context_menu_enabled = true, select_on_context_click = true;
    double structure_gutter_width = 20, structure_gutter_height = 20;
    TableHeaderPresentation header_presentation;
    std::shared_ptr<const IconSource> header_icon_source;
    std::int64_t header_icon_revision = 0;
    friend bool operator==(const TableInput&, const TableInput&) = default;
};
struct TableCellEvent final {
    TablePosition position;
    std::size_t row_index = 0, column_index = 0;
    std::string text, value_type;
};
struct NativeTableCallbacks final {
    std::function<void(std::optional<TableSelection>)> selection_changed;
    std::function<void(const TableCellEvent&)> edited;
    std::function<void(const TableCellEvent&)> submitted;
    std::function<void(const TableCellEvent&, const TableError&)> rejected;
    std::function<void(const TableRange&)> range_values_changed;
    std::function<void(std::size_t, TableSortOrder)> sorted;
    std::function<void(const TableStructureEdit&)> structure_changed;
    std::function<void(std::size_t, double)> column_resized;
    std::function<void(std::size_t, double)> row_resized;
    std::function<void(const TableMergeEdit&)> merge_changed;
};
struct NativeTableClipboard final {
    std::function<ii::core::Result<std::string>()> read;
    std::function<ii::core::Result<void>(std::string)> write;
};

template<class Field,class Menu,class Item,class Header,class Group,class Builder,class Rect,class Image,class Viewport>
struct TableHostPolicy final {
    using TableInput=::lvui::native::TableInput;using Input=TableInput;using TableCellEvent=::lvui::native::TableCellEvent;
    using NativeTableCallbacks=::lvui::native::NativeTableCallbacks;using NativeTableClipboard=::lvui::native::NativeTableClipboard;
    using NativeTableViews=NativeTableViewsValue<Builder>;using NativeInputField=Field;using NativeMenu=Menu;using NativeMenuItem=Item;
    using NativeTableHeaderPresentation=Header;using NativeTableViewGroup=Group;using MenuInput=::lvui::native::MenuInput;
    using MenuItemInput=MenuItemInputValue<Image>;using TableViewItem=TableViewItemValue<Rect>;
    using StyleColor=::lvui::StyleColor;
    using TableAppearance=::lvui::TableAppearance;
    using InputFieldConfiguration=::lvui::InputFieldConfiguration;
    using InputFieldPalette=::lvui::InputFieldPalette;
    using TableError=::lvui::TableError;
    using TableErrorCode=::lvui::TableErrorCode;
    using TableSnapshot=::lvui::TableSnapshot;
    using TableViewportSource=Viewport;
    using TableViewportFrame=::lvui::TableViewportFrameValue<Viewport>;
    using TableViewportAnchor=::lvui::TableViewportAnchor;
    using TableViewportRequest=::lvui::TableViewportRequest;
    using TableViewportMerge=::lvui::TableViewportMerge;
    using TableSelection=::lvui::TableSelection;
    using TableRange=::lvui::TableRange;
    using TablePosition=::lvui::TablePosition;
    using TableViewSnapshot=::lvui::native::TableViewSnapshot;
    using TableSortOrder=::lvui::TableSortOrder;
    using TableSelectionMode=::lvui::TableSelectionMode;
    using TableDirection=::lvui::TableDirection;
    using TableEditing=::lvui::TableEditing;
    using TableEdit=::lvui::TableEdit;
    using TableClipboardData=::lvui::TableClipboardData;
    using TableIndex=::lvui::TableIndex;
    using TableTextSorter=::lvui::TableTextSorter;
    using TableSorter=::lvui::TableSorter;
    using TableMergeEdit=::lvui::TableMergeEdit;
    using TableStructureEdit=::lvui::TableStructureEdit;
    using TableStructureIntent=::lvui::TableStructureIntent;
    using TableStructureAction=::lvui::TableStructureAction;
    using TableDefaultStructureFactory=::lvui::TableDefaultStructureFactory;
    using TableResizeColumns=::lvui::TableResizeColumns;
    using TableResizeRows=::lvui::TableResizeRows;
    [[nodiscard]] static auto parse_table_clipboard(std::string_view text){return ::lvui::parse_table_clipboard(text);}
    [[nodiscard]] static auto format_table_text(std::string_view pattern,std::initializer_list<std::pair<std::string_view,std::string_view>> fields){return ::lvui::detail::format_table_text(pattern,fields);}
    template<class Kind>[[nodiscard]] static bool pointer_canceled(Kind kind)noexcept{return ::lvui::native::pointer_canceled(kind);}
};
template<template<class>class Host,class Field,class Menu,class Item,class Header,class Group,class Builder,class Rect,class Image,class Viewport>
using TableBinding=Host<TableHostPolicy<Field,Menu,Item,Header,Group,Builder,Rect,Image,Viewport>>;
} // namespace lvui::native
