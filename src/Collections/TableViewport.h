#pragma once

#include <LVUI/Table.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lvui {

enum class TableReveal { contain, start, end, center };
struct TableViewportConfiguration final {
    double width = 200, fallback_column_width = 120, minimum_column_width = 32;
    bool fit_flexible_columns = true;
};
struct TableViewportAnchor final {
    std::string row, column;
    std::size_t row_index = 0, column_index = 0;
    double within_y = 0, within_x = 0;
    std::uint64_t vertical_movement = 0, horizontal_movement = 0;
};
struct TableViewportRequest final {
    double height = 200, header_height = 24, x = 0, y = 0;
    std::size_t overscan_rows = 1, overscan_columns = 1;
    std::uint64_t vertical_movement = 0, horizontal_movement = 0;
    TableViewportAnchor anchor;
    std::string reveal_row, reveal_column;
    TableReveal reveal = TableReveal::contain;
};
struct TableViewportSpan final { std::size_t index = 0; double offset = 0, extent = 0; };
struct TableViewportMerge final { TableMerge range; double x = 0, y = 0, width = 0, height = 0; };
template<class Source>
struct TableViewportFrameValue final {
    std::shared_ptr<const Source> source;
    std::vector<TableViewportSpan> rows, columns;
    std::vector<TableViewportMerge> merges;
    double width = 0, height = 0, header_height = 0, content_width = 0, content_height = 0, x = 0, y = 0;
    std::uint64_t source_revision = 0;
    TableViewportAnchor anchor;
};

// Prefix geometry is rebuilt only when the snapshot or layout width changes.
// Scrolling yields independent visible row/column spans rather than allocating
// a view or a cell object for every cell in the source.
struct TableViewportPolicy final {
    using TableSnapshot=::lvui::TableSnapshot;
    using TableIndex=::lvui::TableIndex;
    using TableViewportConfiguration=::lvui::TableViewportConfiguration;
    using TableViewportRequest=::lvui::TableViewportRequest;
    using TableViewportSpan=::lvui::TableViewportSpan;
    using TableReveal=::lvui::TableReveal;
    using TableMerge=::lvui::TableMerge;
    using TableError=::lvui::TableError;
    using TableErrorCode=::lvui::TableErrorCode;
    template<class Source>using Frame=TableViewportFrameValue<Source>;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Engine>using TableViewportBinding=Engine<TableViewportPolicy>;

} // namespace lvui
