#pragma once

#include <LVUI/NativeInputField.h>
#include <LVUI/TableAppearance.h>
#include <LVUI/TableClipboard.h>
#include <LVUI/TableEditing.h>
#include <LVUI/TableSorting.h>
#include <LVUI/TableStructure.h>
#include <LVUI/TableViewport.h>

#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

[[nodiscard]] inline StyleColor table_color(ii::gui::WidgetColor value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
struct TableInput final {
    std::shared_ptr<TableController> controller;
    std::shared_ptr<TableEditor> editor;
    std::shared_ptr<TableOrderEditor> order_editor;
    std::shared_ptr<TableStructureEditor> structure_editor;
    std::shared_ptr<const TableSorter> sorter;
    TableAppearance appearance;
    std::int64_t source_revision = 0;
    bool editable = false, clipboard_enabled = true, sorting_enabled = false;
    bool column_resize_enabled = false, row_resize_enabled = false;
    double column_resize_handle = 6, row_resize_handle = 6;
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
};
struct NativeTableClipboard final {
    std::function<ii::core::Result<std::string>()> read;
    std::function<ii::core::Result<void>(std::string)> write;
};

// The generic host owns clipping, scrolling, pointer and keyboard delivery.
// LVUI owns table data, selection, viewport geometry and retained drawing.
class NativeTable final {
    struct State final {
        struct Resize final {
            bool column = false;
            std::string key;
            std::size_t index = 0;
            double origin = 0, original = 0, preview = 0;
            std::uint64_t source_revision = 0, view_revision = 0;
        };
        TableInput authored;
        std::shared_ptr<const TableSnapshot> snapshot;
        std::shared_ptr<const TableViewportSource> source;
        std::shared_ptr<const TableViewportFrame> frame;
        std::optional<TableSelection> selection;
        std::optional<TableRange> range;
        std::optional<Resize> resize;
        std::optional<std::string> sorted_column, pressed_header, focused_header;
        TableSortOrder sort_order = TableSortOrder::ascending;
        ii::gui::Widget root, header, scroll, body, edit_host;
        std::shared_ptr<TableEditing> editing;
        std::optional<NativeInputField> field;
        std::optional<TableCellEvent> edit_event;
        ii::gui::Rect edit_frame;
        ii::gui::Size extent;
        ii::gui::ScrollViewport viewport;
        TableViewportAnchor anchor;
        NativeTableCallbacks callbacks;
        NativeTableClipboard clipboard;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        double committed_x = 0, committed_y = 0;
        std::uint64_t horizontal_movement = 0, vertical_movement = 0, revision = 0;
        bool live = false, viewport_seen = false, rendering = false, selecting = false, committing = false;
        std::optional<ii::gui::ScrollViewport> pending;
    };
    explicit NativeTable(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Error convert(const TableError& value) {
        const auto code = value.code == TableErrorCode::invalid ? ii::core::ErrorCode::invalid_argument :
                          value.code == TableErrorCode::not_found ? ii::core::ErrorCode::not_found :
                          value.code == TableErrorCode::wrong_thread ? ii::core::ErrorCode::wrong_executor :
                          value.code == TableErrorCode::exhausted ? ii::core::ErrorCode::out_of_range : ii::core::ErrorCode::busy;
        return {code, value.message};
    }
    [[nodiscard]] static bool valid(const TableInput& input) noexcept {
        return input.controller && (!input.editable || input.editor) && (!input.sorting_enabled || input.order_editor) &&
            (!(input.column_resize_enabled || input.row_resize_enabled) || input.structure_editor) &&
            std::isfinite(input.column_resize_handle) && input.column_resize_handle >= 1 && input.column_resize_handle <= 256 &&
            std::isfinite(input.row_resize_handle) && input.row_resize_handle >= 1 && input.row_resize_handle <= 256 &&
            input.source_revision >= 0 && valid_table_appearance(input.appearance);
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value) noexcept {
        const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(channel(value.red)), float(channel(value.green)), float(channel(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static std::string bounded_text(std::string_view input, std::size_t limit) {
        if (input.size() <= limit) return std::string(input);
        auto end = limit; while (end && (static_cast<unsigned char>(input[end]) & 0xc0) == 0x80) --end;
        return std::string(input.substr(0, end));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const TableViewportSource>> source(
            std::shared_ptr<const TableSnapshot> snapshot, const TableAppearance& appearance, double width) {
        auto value = TableViewportSource::create(std::move(snapshot), {.width=width,
            .fallback_column_width=appearance.fallback_column_width, .minimum_column_width=appearance.minimum_column_width,
            .fit_flexible_columns=appearance.fit_flexible_columns});
        if (!value) return std::unexpected(convert(value.error())); return std::move(*value);
    }
    [[nodiscard]] static ii::core::Result<void> selection(const std::shared_ptr<State>& state) {
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto selected = controller->selection(); if (!selected) return std::unexpected(convert(selected.error()));
        auto range = controller->range(); if (!range) return std::unexpected(convert(range.error()));
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (state->revision != revision || state->authored.controller != controller || (*snapshot)->revision != state->snapshot->revision)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during selection");
        state->selection = std::move(*selected); state->range = std::move(*range); return {};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const TableViewportFrame>> plan(
            const std::shared_ptr<State>& state, const ii::gui::ScrollViewport& viewport) {
        TableViewportRequest request{.height=double(viewport.bounds.height), .header_height=0,
            .x=viewport.horizontal_offset, .y=viewport.offset, .overscan_rows=0, .overscan_columns=0,
            .vertical_movement=state->vertical_movement, .horizontal_movement=state->horizontal_movement, .anchor=state->anchor};
        auto result = state->source->frame(request); if (!result) return std::unexpected(convert(result.error()));
        if (result->columns.size() && result->rows.size() > 3072 / result->columns.size())
            return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
        return std::make_shared<const TableViewportFrame>(std::move(*result));
    }
    [[nodiscard]] static ii::core::Result<void> viewport(const std::shared_ptr<State>& state, ii::gui::ScrollViewport value) {
        if (state->rendering) { state->pending = value; return {}; }
        for (unsigned pass = 0; pass < 8; ++pass) {
            if (state->viewport_seen && std::abs(value.offset - state->committed_y) > 1e-6) {
                if (state->vertical_movement == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table vertical movement exhausted");
                ++state->vertical_movement;
            }
            if (state->viewport_seen && std::abs(value.horizontal_offset - state->committed_x) > 1e-6) {
                if (state->horizontal_movement == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table horizontal movement exhausted");
                ++state->horizontal_movement;
            }
            if (value.bounds.width < 1 || value.bounds.height < 1) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table viewport is too small");
            if (state->source->configuration().width != value.bounds.width) {
                auto resized = source(state->snapshot, state->authored.appearance, value.bounds.width); if (!resized) return std::unexpected(resized.error());
                state->source = std::move(*resized);
            }
            state->viewport = value; state->viewport_seen = true; state->pending.reset(); state->rendering = true;
            ii::core::Result<void> applied;
            try {
                auto next = plan(state, value);
                if (!next) applied = std::unexpected(next.error());
                else {
                    state->frame = std::move(*next); state->anchor = state->frame->anchor;
                    state->committed_x = state->frame->x; state->committed_y = state->frame->y;
                    applied = state->body.set_extent({value.bounds.width, value.bounds.height});
                    if (applied) applied = state->scroll.set_virtual_scroll_content(
                        {state->frame->content_width, state->frame->content_height}, state->frame->x, state->frame->y);
                    if (applied && state->editing) applied = position_editor(state);
                    if (applied) applied = state->header.request_repaint();
                    if (applied) applied = state->body.request_repaint();
                }
            } catch (...) { applied = ii::core::fail(ii::core::ErrorCode::failed, "Table viewport update failed", std::current_exception()); }
            state->rendering = false;
            if (!applied) return applied;
            if (!state->pending) return {};
            value = *state->pending; state->pending.reset();
        }
        return ii::core::fail(ii::core::ErrorCode::busy, "Table viewport did not settle");
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_header(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>(); if (!state->frame) return std::shared_ptr<const ii::gui::WidgetDrawing>(output);
        const auto& a = state->authored.appearance; const auto background = color(a.palette.background), divider = color(a.palette.divider), ink = color(a.palette.header_text);
        if (paint.size.width > 0 && paint.size.height > 0) output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        const auto budget = std::max<std::size_t>(4, 128 * 1024 / std::max<std::size_t>(1, state->frame->columns.size()));
        for (const auto& column : state->frame->columns) {
            const auto& data = state->snapshot->columns[column.index];
            const auto left = column.offset + std::min(a.cell_padding, column.extent / 2);
            const bool sorted = state->authored.sorting_enabled && state->sorted_column == data.key;
            const auto indicator = sorted && column.extent >= 24 ? 12. : 0.;
            const auto width = std::max(0., column.extent - 2 * std::min(a.cell_padding, column.extent / 2) - indicator);
            if (!data.title.empty() && width > 0 && paint.size.height > 0) output->texts.push_back({
                .bounds={float(left), 0, float(width), float(paint.size.height)}, .text=bounded_text(data.title, budget), .color=ink,
                .font_size=float(a.font_size), .font_weight=500, .alignment=ii::gui::TextAlignment::left,
                .font_family=paint.font_family, .line_height=float(a.font_size + 3), .elide_right=true,
                .clip=ii::gui::Rect{float(column.offset), 0, float(column.extent), float(paint.size.height)}});
            if (indicator && paint.size.height > 0) output->texts.push_back({
                .bounds={float(column.offset + column.extent - indicator), 0, float(indicator), float(paint.size.height)},
                .text=state->sort_order == TableSortOrder::ascending ? "↑" : "↓", .color=ink,
                .font_size=float(a.font_size), .font_weight=500, .alignment=ii::gui::TextAlignment::center,
                .font_family=paint.font_family, .line_height=float(a.font_size + 3),
                .clip=ii::gui::Rect{float(column.offset), 0, float(column.extent), float(paint.size.height)}});
            if (paint.focused && state->authored.sorting_enabled && state->focused_header == data.key && paint.size.height >= 2) {
                const auto left_edge = std::max(0., column.offset), right_edge = std::min(paint.size.width, column.offset + column.extent);
                if (right_edge - left_edge >= 2) {
                    const auto focus = color(a.palette.current);
                    output->panels.push_back({{float(left_edge), 0, float(right_edge - left_edge), float(paint.size.height)}, focus, focus, 0, 1});
                }
            }
            const auto edge = column.offset + column.extent - 1;
            if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, divider, divider});
        }
        if (state->resize && state->resize->column) for (const auto& column : state->frame->columns)
            if (state->snapshot->columns[column.index].key == state->resize->key) {
                const auto edge = column.offset + state->resize->preview; const auto ink = color(a.palette.current);
                if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, ink, ink});
                break;
            }
        if (paint.size.width > 0 && paint.size.height >= 1) output->panels.push_back({{0, float(paint.size.height - 1), float(paint.size.width), 1}, divider, divider});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_body(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& paint) {
        auto output = std::make_shared<ii::gui::WidgetDrawing>(); if (!state->frame) return std::shared_ptr<const ii::gui::WidgetDrawing>(output);
        const auto& a = state->authored.appearance; const auto background = color(a.palette.background), divider = color(a.palette.divider);
        const auto selection = color(a.palette.selection), current = color(a.palette.current), ink = color(a.palette.cell_text);
        if (paint.size.width > 0 && paint.size.height > 0) output->panels.push_back({{0, 0, float(paint.size.width), float(paint.size.height)}, background, background});
        const auto count = state->frame->rows.size() * state->frame->columns.size();
        const auto budget = std::max<std::size_t>(4, 800 * 1024 / std::max<std::size_t>(1, count));
        for (const auto& row : state->frame->rows) for (const auto& column : state->frame->columns) {
            const auto left = std::max(0., column.offset), top = std::max(0., row.offset);
            const auto right = std::min(paint.size.width, column.offset + column.extent), bottom = std::min(paint.size.height, row.offset + row.extent);
            if (right <= left || bottom <= top) continue;
            const auto& cell = state->snapshot->rows[row.index].cells[column.index];
            const bool selected = a.selection_enabled && state->range && row.index >= state->range->first_row && row.index <= state->range->last_row &&
                                  column.index >= state->range->first_column && column.index <= state->range->last_column;
            if (selected) output->panels.push_back({{float(left), float(top), float(right-left), float(bottom-top)}, selection, selection});
            const auto padding = std::min(a.cell_padding, column.extent / 2), available = column.extent - 2 * padding;
            if (!cell.text.empty() && available > 0) output->texts.push_back({
                .bounds={float(column.offset + padding), float(row.offset), float(available), float(row.extent)},
                .text=bounded_text(cell.text, budget), .color=ink, .font_size=float(a.font_size), .font_weight=500,
                .alignment=ii::gui::TextAlignment::left, .font_family=paint.font_family, .line_height=float(a.font_size + 3),
                .elide_right=true, .clip=ii::gui::Rect{float(left), float(top), float(right-left), float(bottom-top)}});
            if (a.selection_enabled && state->selection && state->selection->current.row == state->snapshot->rows[row.index].key &&
                state->selection->current.column == state->snapshot->columns[column.index].key && right-left >= 2 && bottom-top >= 2)
                output->panels.push_back({{float(left), float(top), float(right-left), float(bottom-top)}, current, current, 0, 1});
        }
        for (const auto& row : state->frame->rows) {
            const auto edge = row.offset + row.extent - 1;
            if (edge >= 0 && edge < paint.size.height && paint.size.width > 0) output->panels.push_back({{0, float(edge), float(paint.size.width), 1}, divider, divider});
        }
        for (const auto& column : state->frame->columns) {
            const auto edge = column.offset + column.extent - 1;
            if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, divider, divider});
        }
        if (state->resize) {
            const auto ink = color(a.palette.current);
            if (state->resize->column) for (const auto& column : state->frame->columns)
                if (state->snapshot->columns[column.index].key == state->resize->key) {
                    const auto edge = column.offset + state->resize->preview;
                    if (edge >= 0 && edge < paint.size.width && paint.size.height > 0) output->panels.push_back({{float(edge), 0, 1, float(paint.size.height)}, ink, ink});
                    break;
                }
            if (!state->resize->column) for (const auto& row : state->frame->rows)
                if (state->snapshot->rows[row.index].key == state->resize->key) {
                    const auto edge = row.offset + state->resize->preview;
                    if (edge >= 0 && edge < paint.size.height && paint.size.width > 0) output->panels.push_back({{0, float(edge), float(paint.size.width), 1}, ink, ink});
                    break;
                }
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(output));
    }
    [[nodiscard]] static ii::core::Result<void> layout(const std::shared_ptr<State>& state) {
        const auto width = state->extent.width, height = state->extent.height;
        const auto header = std::min(state->authored.appearance.header_height, std::max(0., height - 1));
        if (width < 1 || height < 2) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table extent is too small");
        if (auto result = state->header.set_frame({0, 0, float(width), float(header)}); !result) return result;
        return state->scroll.set_frame({0, float(header), float(width), float(height - header)});
    }
    [[nodiscard]] static ii::core::Result<void> refresh(const std::shared_ptr<State>& state) {
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (state->revision != revision || state->authored.controller != controller) return {};
        auto next = source(*snapshot, state->authored.appearance, std::max(1., state->extent.width)); if (!next) return std::unexpected(next.error());
        auto previous_snapshot = state->snapshot; auto previous_source = state->source; auto previous_frame = state->frame;
        auto previous_selection = state->selection; auto previous_range = state->range; const auto previous_anchor = state->anchor;
        auto previous_sorted = state->sorted_column;
        const auto previous_viewport = state->viewport; const auto previous_x = state->committed_x, previous_y = state->committed_y;
        const auto previous_h = state->horizontal_movement, previous_v = state->vertical_movement;
        const auto rollback = [&] { if (state->revision == revision && state->authored.controller == controller) {
            state->snapshot = std::move(previous_snapshot); state->source = std::move(previous_source); state->frame = std::move(previous_frame);
            state->selection = std::move(previous_selection); state->range = std::move(previous_range); state->anchor = previous_anchor;
            state->sorted_column = std::move(previous_sorted);
            state->viewport = previous_viewport; state->committed_x = previous_x; state->committed_y = previous_y;
            state->horizontal_movement = previous_h; state->vertical_movement = previous_v;
            (void)state->header.request_repaint(); (void)state->body.request_repaint();
        }};
        if ((*snapshot)->revision != state->snapshot->revision) state->sorted_column.reset();
        state->snapshot = std::move(*snapshot); state->source = std::move(*next);
        if (auto selected = selection(state); !selected) { rollback(); return selected; }
        if (state->viewport_seen) {
            auto rendered = viewport(state, state->viewport);
            if (!rendered) { rollback(); return rendered; }
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> publish_selection(const std::shared_ptr<State>& state) {
        if (auto selected = selection(state); !selected) return selected;
        if (auto repainted = state->body.request_repaint(); !repainted) return repainted;
        if (state->live && state->callbacks.selection_changed) { auto callback = state->callbacks.selection_changed; callback(state->selection); }
        return {};
    }
    [[nodiscard]] static std::optional<ii::gui::Rect> editor_rect(const std::shared_ptr<State>& state, const TablePosition& position) {
        if (!state->frame) return std::nullopt;
        const auto row = std::ranges::find_if(state->frame->rows, [&](const auto& value) { return state->snapshot->rows[value.index].key == position.row; });
        const auto column = std::ranges::find_if(state->frame->columns, [&](const auto& value) { return state->snapshot->columns[value.index].key == position.column; });
        if (row == state->frame->rows.end() || column == state->frame->columns.end()) return std::nullopt;
        const auto x = std::max(0., column->offset), y = std::max(0., row->offset);
        const auto right = std::min(state->frame->width, column->offset + column->extent);
        const auto bottom = std::min(state->frame->height, row->offset + row->extent);
        if (right - x < 1 || bottom - y < 1) return std::nullopt;
        return ii::gui::Rect{float(x), float(y), float(right - x), float(bottom - y)};
    }
    [[nodiscard]] static ii::core::Result<void> clear_edit(const std::shared_ptr<State>& state, bool focus) {
        auto editing = std::exchange(state->editing, {}); state->field.reset(); state->edit_event.reset(); state->failure.reset();
        if (editing) (void)editing->cancel();
        if (auto removed = state->body.set_escape_action({}); !removed) return removed;
        if (auto hidden = state->edit_host.set_visible(false); !hidden) return hidden;
        if (auto cleared = state->edit_host.clear_content(); !cleared) return cleared;
        if (focus && state->live) return state->body.focus(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> position_editor(const std::shared_ptr<State>& state) {
        if (!state->editing || !state->edit_event) return {};
        const auto rect = editor_rect(state, state->edit_event->position);
        if (!rect) return clear_edit(state, false);
        state->edit_frame = *rect; return state->edit_host.set_frame(*rect);
    }
    [[nodiscard]] static ii::core::Result<void> commit_edit(const std::shared_ptr<State>& state) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!state->editing || !state->edit_event) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "No table cell is being edited");
        if (state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "Table edit is committing");
        auto editing = state->editing; auto editor = state->authored.editor; auto controller = state->authored.controller;
        if (state->field) {
            auto field = state->field->snapshot(); if (!field) return std::unexpected(field.error());
            if (!state->live || state->editing != editing || state->authored.editor != editor || state->authored.controller != controller)
                return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during its edit");
            auto synchronized = editing->set_draft(std::move(field->text)); if (!synchronized) return std::unexpected(convert(synchronized.error()));
        }
        auto value = editing->state(); if (!value) return std::unexpected(convert(value.error()));
        auto event = *state->edit_event; event.text = value->draft;
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        auto result = editing->commit();
        if (!state->live || state->editing != editing || state->authored.editor != editor || state->authored.controller != controller) return {};
        if (!result) {
            state->failure = convert(result.error());
            if (state->callbacks.rejected) { auto callback = state->callbacks.rejected; callback(event, result.error()); }
            return std::unexpected(convert(result.error()));
        }
        if (auto cleared = clear_edit(state, true); !cleared) return cleared;
        if (!state->live || state->authored.editor != editor || state->authored.controller != controller) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        if (state->live && state->authored.editor == editor && state->authored.controller == controller && state->callbacks.submitted) {
            auto callback = state->callbacks.submitted; callback(event);
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> begin_edit(const std::shared_ptr<State>& state) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto current = state->root.snapshot(); !current) return std::unexpected(current.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.editable || !state->authored.editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table editing is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.editor; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        }
        auto selected = controller->selection(); if (!selected) return std::unexpected(convert(selected.error()));
        if (!*selected) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Select a table cell before editing");
        auto candidate = TableEditing::begin(*snapshot, editor, (*selected)->current); if (!candidate) return std::unexpected(convert(candidate.error()));
        auto value = candidate->state(); if (!value) return std::unexpected(convert(value.error()));
        auto row = std::ranges::find_if((*snapshot)->rows, [&](const auto& item) { return item.key == value->position.row; });
        auto column = std::ranges::find_if((*snapshot)->columns, [&](const auto& item) { return item.key == value->position.column; });
        const auto row_index = std::size_t(row - (*snapshot)->rows.begin()), column_index = std::size_t(column - (*snapshot)->columns.begin());
        TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
            .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
            .reveal_row=value->position.row, .reveal_column=value->position.column};
        auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
        if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return scrolled;
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before editing");
        const auto rect = editor_rect(state, value->position); if (!rect) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table edit cell is not visible");
        if (auto frame = state->edit_host.set_frame(*rect); !frame) return frame;
        InputFieldConfiguration field; field.text = value->draft; field.width = rect->width; field.height = rect->height;
        field.horizontal_padding = std::min(state->authored.appearance.cell_padding, double(rect->width) / 2);
        field.corner_radius = 0; field.font_size = state->authored.appearance.font_size; field.accessibility_name = "Edit table cell";
        field.palette.background = state->authored.appearance.palette.background; field.palette.text = state->authored.appearance.palette.cell_text;
        field.palette.focus = state->authored.appearance.palette.current;
        auto staged = std::make_shared<std::optional<NativeInputField>>(); std::weak_ptr<State> weak = state;
        auto mounted = state->edit_host.set_content([weak, field, staged](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
            auto widget = NativeInputField::create(builder, {}, "field", field, {
                .edited = [weak](std::string text) { if (auto state = weak.lock(); state && state->live && state->editing && state->edit_event) try {
                    auto editing = state->editing; auto changed = editing->set_draft(text);
                    if (!changed) { state->failure = convert(changed.error()); return; }
                    if (*changed && state->callbacks.edited) { auto event = *state->edit_event; event.text = std::move(text); auto callback = state->callbacks.edited; callback(event); }
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table draft callback failed", std::current_exception()}; } },
                .submitted = [weak] { if (auto state = weak.lock(); state && state->live && state->editing) try {
                    if (auto result = commit_edit(state); !result) state->failure = result.error();
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table commit callback failed", std::current_exception()}; } },
                .focus_changed = [weak](bool focused) { if (!focused) if (auto state = weak.lock(); state && state->live && state->editing && !state->committing) try {
                    if (auto result = clear_edit(state, false); !result) state->failure = result.error();
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table edit cancellation failed", std::current_exception()}; } }
            });
            if (!widget) return std::unexpected(widget.error()); *staged = *widget; return {};
        });
        if (!mounted) return mounted;
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.editor != editor || !*staged) {
            (void)state->edit_host.clear_content(); return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while opening its editor");
        }
        state->editing = std::make_shared<TableEditing>(std::move(*candidate)); state->field = **staged; state->edit_frame = *rect;
        state->edit_event = TableCellEvent{value->position, row_index, column_index, value->draft, value->value_type};
        if (auto escape = state->body.set_escape_action([weak] { if (auto state = weak.lock(); state && state->live && state->editing) try {
                if (auto result = clear_edit(state, true); !result) state->failure = result.error();
            } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table edit cancellation failed", std::current_exception()}; }
        }); !escape) { (void)clear_edit(state, false); return escape; }
        if (auto shown = state->edit_host.set_visible(true); !shown) { (void)clear_edit(state, false); return shown; }
        if (auto focused = state->field->root().focus(); !focused) { (void)clear_edit(state, false); return focused; }
        return {};
    }
    enum class ClipboardOperation { copy, cut, paste };
    [[nodiscard]] static ii::core::Result<void> clipboard(const std::shared_ptr<State>& state, ClipboardOperation operation,
            std::optional<std::string> content = std::nullopt) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.appearance.selection_enabled || !state->authored.clipboard_enabled)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table clipboard is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        if (operation != ClipboardOperation::copy && (!state->authored.editable || !state->authored.editor))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table clipboard editing is unavailable");
        const auto revision = state->revision; auto controller = state->authored.controller; auto editor = state->authored.editor;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        auto range = controller->range(); if (!range) return std::unexpected(convert(range.error()));
        auto after_selection = controller->source_snapshot(); if (!after_selection) return std::unexpected(convert(after_selection.error()));
        const auto current = [&] { return state->live && state->revision == revision && state->authored.controller == controller &&
            (operation == ClipboardOperation::copy || (state->authored.editor == editor && state->authored.editable)); };
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed during clipboard access");
        if ((*snapshot)->revision != (*after_selection)->revision || (*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before clipboard access");
        }
        if (!*range) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Select table cells before using the clipboard");
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        std::optional<TableEdit> edit; TableRange affected = **range;
        if (operation == ClipboardOperation::cut) {
            auto prepared = prepare_table_paste(*snapshot, affected, TableClipboardData{{{std::string{}}}});
            if (!prepared) return std::unexpected(convert(prepared.error())); edit = std::move(*prepared);
        }
        if (operation != ClipboardOperation::paste) {
            auto text = serialize_table_clipboard(*snapshot, affected); if (!text) return std::unexpected(convert(text.error()));
            auto write = state->clipboard.write;
            auto result = write ? write(std::move(*text)) : ii::gui::write_clipboard({.text=std::move(*text)});
            if (!result) return result;
            if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while copying");
            if (operation == ClipboardOperation::copy) return {};
        } else {
            if (!content) {
                auto read = state->clipboard.read;
                if (read) { auto result = read(); if (!result) return std::unexpected(result.error()); content = std::move(*result); }
                else { auto result = ii::gui::read_clipboard(); if (!result) return std::unexpected(result.error()); content = std::move(result->text); }
            }
            if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while reading the clipboard");
            auto parsed = parse_table_clipboard(*content); if (!parsed) return std::unexpected(convert(parsed.error()));
            auto prepared = prepare_table_paste(*snapshot, affected, *parsed); if (!prepared) return std::unexpected(convert(prepared.error()));
            auto index = TableIndex::create(*snapshot); if (!index) return std::unexpected(convert(index.error()));
            affected.last_row = *index->row(prepared->cells.back().row);
            affected.last_column = *index->column(prepared->cells.back().column);
            edit = std::move(*prepared);
        }
        auto submitted = editor->edit(*edit);
        if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.editor == editor; };
        if (!compatible() || *submitted == (*snapshot)->revision) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted && state->callbacks.range_values_changed) {
            auto callback = state->callbacks.range_values_changed; callback(affected);
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<void> sort(const std::shared_ptr<State>& state, std::string column, TableSortOrder direction) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.sorting_enabled || !state->authored.order_editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table sorting is unavailable");
        if (state->editing || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.order_editor; auto sorter = state->authored.sorter;
        const auto revision = state->revision;
        const auto current = [&] { return state->live && state->revision == revision && state->authored.controller == controller &&
            state->authored.order_editor == editor && state->authored.sorter == sorter && state->authored.sorting_enabled; };
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before sorting");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before sorting");
        }
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        TableTextSorter fallback; const auto& policy = sorter ? *sorter : static_cast<const TableSorter&>(fallback);
        auto prepared = prepare_table_sort(*snapshot, std::move(column), direction, policy);
        if (!prepared) return std::unexpected(convert(prepared.error()));
        if (!current()) return ii::core::fail(ii::core::ErrorCode::busy, "Table changed while preparing a sort");
        auto index = TableIndex::create(*snapshot); if (!index) return std::unexpected(convert(index.error()));
        const auto column_index = *index->column(prepared->column);
        auto submitted = editor->reorder(*prepared); if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.order_editor == editor &&
            state->authored.sorter == sorter && state->authored.sorting_enabled; };
        if (!compatible()) return {};
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted) {
            state->sorted_column = prepared->column; state->sort_order = direction;
            if (auto repainted = state->header.request_repaint(); !repainted && refreshed) refreshed = repainted;
            if (state->callbacks.sorted) { auto callback = state->callbacks.sorted; callback(column_index, direction); }
        }
        if (!refreshed) return refreshed; state->failure.reset(); return {};
    }
    [[nodiscard]] static ii::core::Result<std::uint64_t> structure(const std::shared_ptr<State>& state, TableStructureEdit edit) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state->live || !state->authored.appearance.enabled || !state->authored.structure_editor)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Table structure editing is unavailable");
        if (state->editing || state->resize || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; auto editor = state->authored.structure_editor; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller || state->authored.structure_editor != editor)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its structure edit");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return std::unexpected(refreshed.error());
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before its structure edit");
        }
        { auto candidate = prepare_table_structure(*snapshot, edit); if (!candidate) return std::unexpected(convert(candidate.error())); }
        state->committing = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{state->committing};
        auto submitted = editor->apply(edit); if (!submitted) return std::unexpected(convert(submitted.error()));
        const auto compatible = [&] { return state->live && state->authored.controller == controller && state->authored.structure_editor == editor; };
        if (!compatible() || *submitted == (*snapshot)->revision) return *submitted;
        auto refreshed = refresh(state); if (!refreshed) state->failure = refreshed.error();
        auto published = controller->source_snapshot();
        if (compatible() && published && (*published)->revision == *submitted && state->callbacks.structure_changed) {
            auto callback = state->callbacks.structure_changed; callback(edit);
        }
        if (!refreshed) return std::unexpected(refreshed.error()); state->failure.reset(); return *submitted;
    }
    [[nodiscard]] static ii::core::Result<void> repaint_resize(const std::shared_ptr<State>& state) {
        if (auto header = state->header.request_repaint(); !header) return header;
        return state->body.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> clear_resize(const std::shared_ptr<State>& state) {
        if (!state->resize) return {};
        const bool column = state->resize->column; state->resize.reset();
        if (auto removed = (column ? state->header : state->body).set_escape_action({}); !removed) return removed;
        return repaint_resize(state);
    }
    [[nodiscard]] static ii::core::Result<void> begin_resize(const std::shared_ptr<State>& state, bool column,
            std::string key, std::size_t index, double origin, double size) {
        if (state->editing || state->resize || state->committing) return ii::core::fail(ii::core::ErrorCode::busy, "A table edit is already active");
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller)
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before resizing");
        if ((*snapshot)->revision != state->snapshot->revision) {
            if (auto refreshed = refresh(state); !refreshed) return refreshed;
            return ii::core::fail(ii::core::ErrorCode::busy, "Table changed before resizing");
        }
        state->resize = State::Resize{column, std::move(key), index, origin, size, size, (*snapshot)->revision, revision};
        std::weak_ptr<State> weak = state;
        if (auto escape = (column ? state->header : state->body).set_escape_action([weak] { if (auto state = weak.lock(); state && state->live && state->resize) try {
                if (auto result = clear_resize(state); !result) state->failure = result.error();
            } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table resize cancellation failed", std::current_exception()}; }
        }); !escape) { state->resize.reset(); return escape; }
        auto painted = repaint_resize(state); if (!painted) (void)clear_resize(state); return painted;
    }
    [[nodiscard]] static ii::core::Result<bool> resize_input(const std::shared_ptr<State>& state,
            const ii::gui::WidgetInput& event, bool column) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->resize || state->resize->column != column) return false;
        if (event.kind == WidgetInputKind::focus_lost || (event.kind == WidgetInputKind::key_down && event.key == WidgetKey::escape) ||
            state->revision != state->resize->view_revision) {
            if (auto cleared = clear_resize(state); !cleared) return std::unexpected(cleared.error()); return true;
        }
        if (event.kind != WidgetInputKind::pointer_move && event.kind != WidgetInputKind::pointer_up) return true;
        if (event.local_position) {
            const auto position = column ? event.local_position->x : event.local_position->y;
            state->resize->preview = std::clamp(state->resize->original + position - state->resize->origin, column ? 32. : 18., 8192.);
        }
        if (event.kind == WidgetInputKind::pointer_move) {
            if (auto painted = repaint_resize(state); !painted) return std::unexpected(painted.error()); return true;
        }
        auto completed = *state->resize;
        if (auto cleared = clear_resize(state); !cleared) return std::unexpected(cleared.error());
        if (std::abs(completed.preview - completed.original) < 1e-6) return true;
        TableStructureEdit edit{.revision=completed.source_revision};
        if (column) edit.operations.emplace_back(TableResizeColumns{{{completed.key, completed.preview}}});
        else edit.operations.emplace_back(TableResizeRows{{{completed.key, completed.preview}}});
        auto controller = state->authored.controller; auto editor = state->authored.structure_editor;
        auto submitted = structure(state, std::move(edit)); if (!submitted) return std::unexpected(submitted.error());
        auto published = controller->source_snapshot();
        if (state->live && state->authored.controller == controller && state->authored.structure_editor == editor &&
            published && (*published)->revision == *submitted && *submitted != completed.source_revision) {
            auto callback = column ? state->callbacks.column_resized : state->callbacks.row_resized;
            if (callback) callback(completed.index, completed.preview);
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> header_input(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live) return false;
        if (state->resize && state->resize->column) return resize_input(state, event, true);
        if (event.kind == WidgetInputKind::focus_lost || event.kind == WidgetInputKind::pointer_leave) {
            state->pressed_header.reset(); return false;
        }
        if (event.kind == WidgetInputKind::key_down && state->authored.appearance.enabled && state->authored.sorting_enabled &&
            !event.alt && !event.control && !event.meta) {
            const auto& columns = state->snapshot->columns;
            std::optional<std::size_t> selected;
            for (std::size_t i = 0; i < columns.size(); ++i) if (columns[i].enabled && state->focused_header == columns[i].key) { selected = i; break; }
            if (!selected) for (std::size_t i = 0; i < columns.size(); ++i) if (columns[i].enabled) { selected = i; break; }
            if (!selected) return false;
            if (event.key == WidgetKey::left || event.key == WidgetKey::right || event.key == WidgetKey::home || event.key == WidgetKey::end) {
                const auto direction = event.key == WidgetKey::left || event.key == WidgetKey::end ? -1 : 1;
                auto position = event.key == WidgetKey::home ? std::ptrdiff_t(-1) :
                    event.key == WidgetKey::end ? std::ptrdiff_t(columns.size()) : std::ptrdiff_t(*selected);
                for (position += direction; position >= 0 && position < std::ptrdiff_t(columns.size()); position += direction)
                    if (columns[std::size_t(position)].enabled) { selected = std::size_t(position); break; }
                state->focused_header = columns[*selected].key;
                TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
                    .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
                    .reveal_column=*state->focused_header};
                auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
                if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return std::unexpected(scrolled.error());
                if (auto painted = state->header.request_repaint(); !painted) return std::unexpected(painted.error()); return true;
            }
            if (event.key == WidgetKey::enter || event.key == WidgetKey::space) {
                const auto key = columns[*selected].key; state->focused_header = key;
                const auto order = state->sorted_column == key && state->sort_order == TableSortOrder::ascending ?
                    TableSortOrder::descending : TableSortOrder::ascending;
                if (auto result = sort(state, key, order); !result) return std::unexpected(result.error()); return true;
            }
            return false;
        }
        if (event.kind != WidgetInputKind::pointer_down && event.kind != WidgetInputKind::pointer_up) return false;
        if (event.kind == WidgetInputKind::pointer_down && state->authored.appearance.enabled && state->authored.column_resize_enabled &&
            event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (y >= 0 && y < state->authored.appearance.header_height) for (const auto& column : state->frame->columns) {
                const auto edge = column.offset + column.extent;
                if (x >= edge - state->authored.column_resize_handle && x < edge && state->snapshot->columns[column.index].enabled) {
                    state->pressed_header.reset();
                    if (auto started = begin_resize(state, true, state->snapshot->columns[column.index].key, column.index, x, column.extent); !started)
                        return std::unexpected(started.error());
                    if (auto focused = state->header.focus(); !focused) { (void)clear_resize(state); return std::unexpected(focused.error()); }
                    return true;
                }
            }
        }
        if (!state->authored.appearance.enabled || !state->authored.sorting_enabled) { state->pressed_header.reset(); return true; }
        std::optional<std::string> key;
        if (event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (y >= 0 && y < state->authored.appearance.header_height)
                for (const auto& visible : state->frame->columns) if (x >= visible.offset && x < visible.offset + visible.extent) {
                    const auto& column = state->snapshot->columns[visible.index]; if (column.enabled) key = column.key; break;
                }
        }
        if (event.kind == WidgetInputKind::pointer_down) {
            state->pressed_header = key;
            if (key) { state->focused_header = key; if (auto focused = state->header.focus(); !focused) return std::unexpected(focused.error()); }
            return true;
        }
        auto pressed = std::exchange(state->pressed_header, {});
        if (!pressed || pressed != key) return true;
        const auto order = state->sorted_column == key && state->sort_order == TableSortOrder::ascending ?
            TableSortOrder::descending : TableSortOrder::ascending;
        if (auto result = sort(state, *key, order); !result) return std::unexpected(result.error());
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using ii::gui::WidgetInputKind; using ii::gui::WidgetKey;
        if (!state->live || !state->authored.appearance.enabled) return false;
        if (state->resize && !state->resize->column) return resize_input(state, event, false);
        if (event.kind == WidgetInputKind::focus_lost) { state->selecting = false; return false; }
        if (state->editing) {
            if (event.kind == WidgetInputKind::key_down && event.key == WidgetKey::escape) {
                if (auto canceled = clear_edit(state, true); !canceled) return std::unexpected(canceled.error()); return true;
            }
            if (event.kind == WidgetInputKind::pointer_down && event.local_position) {
                const auto x = event.local_position->x, y = event.local_position->y; const auto frame = state->edit_frame;
                if (x < frame.x || y < frame.y || x >= frame.x + frame.width || y >= frame.y + frame.height) {
                    if (auto canceled = clear_edit(state, false); !canceled) return std::unexpected(canceled.error());
                } else return false;
            } else return false;
        }
        const bool pointer = event.kind == WidgetInputKind::pointer_down || event.kind == WidgetInputKind::pointer_move || event.kind == WidgetInputKind::pointer_up;
        const bool copy_key = event.kind == WidgetInputKind::key_down && (event.key == WidgetKey::copy ||
            ((event.control || event.meta) && (event.text == "c" || event.text == "C")));
        const bool cut_key = event.kind == WidgetInputKind::key_down && (event.key == WidgetKey::cut ||
            ((event.control || event.meta) && (event.text == "x" || event.text == "X")));
        const bool paste_key = event.kind == WidgetInputKind::key_down && (event.control || event.meta) &&
            (event.text == "v" || event.text == "V");
        const bool paste_content = event.kind == WidgetInputKind::text && bool(event.content);
        const bool keyboard = event.kind == WidgetInputKind::key_down &&
            (state->authored.appearance.keyboard_navigation || (state->authored.editable && event.key == WidgetKey::enter) ||
             (state->authored.clipboard_enabled && (copy_key || cut_key || paste_key)));
        if (!pointer && !keyboard && !(state->authored.clipboard_enabled && paste_content)) return false;
        auto controller = state->authored.controller; const auto revision = state->revision;
        auto latest = controller->source_snapshot(); if (!latest) return std::unexpected(convert(latest.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        if ((*latest)->revision != state->snapshot->revision) {
            state->selecting = false; if (auto result = refresh(state); !result) return std::unexpected(result.error()); return true;
        }
        if (event.kind == WidgetInputKind::pointer_down && state->authored.row_resize_enabled && event.local_position && state->frame) {
            const auto x = event.local_position->x, y = event.local_position->y;
            if (x >= 0 && x < state->frame->width) for (const auto& row : state->frame->rows) {
                const auto edge = row.offset + row.extent;
                if (y >= edge - state->authored.row_resize_handle && y < edge && state->snapshot->rows[row.index].enabled) {
                    if (auto started = begin_resize(state, false, state->snapshot->rows[row.index].key, row.index, y, row.extent); !started)
                        return std::unexpected(started.error());
                    if (auto focused = state->body.focus(); !focused) { (void)clear_resize(state); return std::unexpected(focused.error()); }
                    return true;
                }
            }
        }
        if (!state->authored.appearance.selection_enabled) return pointer && event.kind == WidgetInputKind::pointer_down;
        if (state->authored.clipboard_enabled && (copy_key || cut_key || paste_key || paste_content)) {
            const auto operation = copy_key ? ClipboardOperation::copy : cut_key ? ClipboardOperation::cut : ClipboardOperation::paste;
            auto result = clipboard(state, operation, paste_content ? std::optional<std::string>(event.content->text) : std::nullopt);
            if (!result) return std::unexpected(result.error()); return true;
        }
        if (pointer) {
            if (event.kind == WidgetInputKind::pointer_up) { const bool had = state->selecting; state->selecting = false; return had; }
            if (event.kind == WidgetInputKind::pointer_move && !state->selecting) return false;
            if (!event.local_position || !state->frame) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            const auto x = event.local_position->x, y = event.local_position->y;
            auto row = std::ranges::find_if(state->frame->rows, [&](const auto& value) { return y >= value.offset && y < value.offset + value.extent; });
            auto column = std::ranges::find_if(state->frame->columns, [&](const auto& value) { return x >= value.offset && x < value.offset + value.extent; });
            if (row == state->frame->rows.end() || column == state->frame->columns.end()) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            const auto& row_data = state->snapshot->rows[row->index]; const auto& column_data = state->snapshot->columns[column->index];
            if (!row_data.enabled || !column_data.enabled || !row_data.cells[column->index].enabled) return event.kind == WidgetInputKind::pointer_down || state->selecting;
            const TablePosition position{row_data.key, column_data.key};
            auto changed = controller->select(position, event.kind == WidgetInputKind::pointer_move || event.shift);
            if (!changed) return std::unexpected(convert(changed.error()));
            if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
            state->selecting = true;
            if (auto focused = state->body.focus(); !focused) return std::unexpected(focused.error());
            if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
            if (*changed) if (auto published = publish_selection(state); !published) return std::unexpected(published.error());
            return true;
        }
        if (event.alt || event.control || event.meta) return false;
        std::optional<TableDirection> direction;
        if (event.key == WidgetKey::up) direction = TableDirection::up; else if (event.key == WidgetKey::down) direction = TableDirection::down;
        else if (event.key == WidgetKey::left) direction = TableDirection::left; else if (event.key == WidgetKey::right) direction = TableDirection::right;
        else if (event.key == WidgetKey::home) direction = TableDirection::home; else if (event.key == WidgetKey::end) direction = TableDirection::end;
        if (!direction) {
            if (event.key == WidgetKey::enter && state->authored.editable) {
                auto started = begin_edit(state);
                if (!started && started.error().code != ii::core::ErrorCode::invalid_argument && started.error().code != ii::core::ErrorCode::not_found)
                    return std::unexpected(started.error());
                return true;
            }
            return false;
        }
        auto moved = controller->navigate(*direction, event.shift); if (!moved) return std::unexpected(convert(moved.error()));
        if (!state->live || state->revision != revision || state->authored.controller != controller) return true;
        if (*moved) {
            if (auto published = publish_selection(state); !published) return std::unexpected(published.error());
            if (!state->live || state->revision != revision || state->authored.controller != controller || !state->selection) return true;
            TableViewportRequest reveal{.height=double(state->viewport.bounds.height), .header_height=0,
                .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0,
                .reveal_row=state->selection->current.row, .reveal_column=state->selection->current.column};
            auto target = state->source->frame(reveal); if (!target) return std::unexpected(convert(target.error()));
            if (auto scrolled = state->scroll.set_scroll_position(target->x, target->y); !scrolled) return std::unexpected(scrolled.error());
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, TableInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!valid(input)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid table configuration");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->revision == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Table revision exhausted");
            const auto observed = state->revision; auto controller = input.controller;
            auto snapshot = controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
            if (state->revision != observed) return {};
            if (state->authored == input && state->snapshot == *snapshot) return {};
            auto next = source(*snapshot, input.appearance, input.appearance.width); if (!next) return std::unexpected(next.error());
            auto candidate = (*next)->frame({.height=input.appearance.height - input.appearance.header_height, .header_height=0,
                .x=state->viewport.horizontal_offset, .y=state->viewport.offset, .overscan_rows=0, .overscan_columns=0});
            if (!candidate) return std::unexpected(convert(candidate.error()));
            if (candidate->columns.size() && candidate->rows.size() > 3072 / candidate->columns.size())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
            if (state->editing && (state->authored.controller != input.controller || state->authored.editor != input.editor ||
                                   !input.editable || !input.appearance.enabled)) {
                if (auto canceled = clear_edit(state, false); !canceled) return canceled;
                if (state->revision != observed) return {};
            }
            if (state->resize) { if (auto cleared = clear_resize(state); !cleared) return cleared; if (state->revision != observed) return {}; }
            auto previous = state->authored; auto previous_snapshot = state->snapshot; auto previous_source = state->source; auto previous_frame = state->frame;
            auto previous_selection = state->selection; auto previous_range = state->range; const auto previous_extent = state->extent;
            auto previous_sorted = state->sorted_column; auto previous_pressed = state->pressed_header;
            const auto previous_anchor = state->anchor; const auto previous_viewport = state->viewport;
            const auto previous_x = state->committed_x, previous_y = state->committed_y;
            const auto previous_h = state->horizontal_movement, previous_v = state->vertical_movement;
            const auto revision = ++state->revision; state->authored = std::move(input); state->snapshot = std::move(*snapshot); state->source = std::move(*next);
            if (previous_snapshot->revision != state->snapshot->revision || previous.controller != state->authored.controller ||
                previous.order_editor != state->authored.order_editor || previous.sorter != state->authored.sorter || !state->authored.sorting_enabled) {
                state->sorted_column.reset(); state->pressed_header.reset();
            }
            state->extent = {state->authored.appearance.width, state->authored.appearance.height};
            auto rollback = [&] { if (state->revision == revision) { state->authored = std::move(previous); state->snapshot = std::move(previous_snapshot);
                state->source = std::move(previous_source); state->frame = std::move(previous_frame); state->selection = std::move(previous_selection);
                state->range = std::move(previous_range); state->extent = previous_extent; state->anchor = previous_anchor; state->viewport = previous_viewport;
                state->sorted_column = std::move(previous_sorted); state->pressed_header = std::move(previous_pressed);
                state->committed_x = previous_x; state->committed_y = previous_y; state->horizontal_movement = previous_h; state->vertical_movement = previous_v;
                (void)state->root.set_extent(state->extent); (void)layout(state); (void)state->root.set_enabled(state->authored.appearance.enabled);
                (void)state->body.set_accessibility_semantics(ii::gui::AccessibilitySemantics{.name=state->authored.appearance.accessibility_name});
                if (state->frame) (void)state->scroll.set_scroll_options({.width=state->extent.width,
                    .height=state->extent.height - state->authored.appearance.header_height, .enabled=true,
                    .show_indicator=state->authored.appearance.show_indicator, .virtual_content_height=state->frame->content_height,
                    .virtual_content_width=state->frame->content_width, .horizontal_enabled=true});
                (void)state->header.request_repaint(); (void)state->body.request_repaint(); } };
            if (auto selected = selection(state); !selected) { rollback(); return selected; }
            if (auto resized = state->root.set_extent(state->extent); !resized) { rollback(); return resized; }
            if (auto arranged = layout(state); !arranged) { rollback(); return arranged; }
            if (state->revision != revision) return {};
            if (state->viewport_seen) if (auto rendered = viewport(state, state->viewport); !rendered) { rollback(); return rendered; }
            auto options = ii::gui::ScrollOptions{.width=state->extent.width,
                .height=state->extent.height - state->authored.appearance.header_height, .enabled=true,
                .show_indicator=state->authored.appearance.show_indicator,
                .virtual_content_height=state->frame ? state->frame->content_height : 0,
                .virtual_content_width=state->frame ? state->frame->content_width : 0, .horizontal_enabled=true};
            if (auto changed = state->scroll.set_scroll_options(options); !changed) { rollback(); return changed; }
            if (auto enabled = state->root.set_enabled(state->authored.appearance.enabled); !enabled) { rollback(); return enabled; }
            if (auto semantics = state->body.set_accessibility_semantics(ii::gui::AccessibilitySemantics{
                    .name=state->authored.appearance.accessibility_name}); !semantics) { rollback(); return semantics; }
            if (!state->authored.appearance.enabled || !state->authored.appearance.selection_enabled) state->selecting = false;
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Table update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<NativeTable> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, TableInput input, NativeTableCallbacks callbacks = {}, NativeTableClipboard clipboard = {}) {
        if (!valid(input)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid table configuration");
        try {
            auto snapshot = input.controller->source_snapshot(); if (!snapshot) return std::unexpected(convert(snapshot.error()));
            auto prepared = source(*snapshot, input.appearance, input.appearance.width); if (!prepared) return std::unexpected(prepared.error());
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->snapshot = std::move(*snapshot);
            state->source = std::move(*prepared); state->callbacks = std::move(callbacks); state->clipboard = std::move(clipboard);
            state->extent = {state->authored.appearance.width, state->authored.appearance.height};
            if (auto selected = selection(state); !selected) return std::unexpected(selected.error());
            std::weak_ptr<State> weak = state;
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            auto header = builder.absolute_stack(*root, std::string(id) + ".header", {state->extent.width, state->authored.appearance.header_height});
            if (!header) return std::unexpected(header.error()); state->header = *header;
            auto initial = state->source->frame({.height=state->extent.height - state->authored.appearance.header_height, .header_height=0,
                                                  .overscan_rows=0, .overscan_columns=0});
            if (!initial) return std::unexpected(convert(initial.error())); state->frame = std::make_shared<const TableViewportFrame>(std::move(*initial));
            if (state->frame->columns.size() && state->frame->rows.size() > 3072 / state->frame->columns.size())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Visible table exceeds the host drawing budget");
            auto scroll = builder.scroll_view(*root, std::string(id) + ".scroll", {.width=state->extent.width,
                .height=state->extent.height - state->authored.appearance.header_height, .enabled=true,
                .show_indicator=state->authored.appearance.show_indicator, .virtual_content_height=state->frame->content_height,
                .virtual_content_width=state->frame->content_width, .horizontal_enabled=true});
            if (!scroll) return std::unexpected(scroll.error()); state->scroll = *scroll;
            auto body = builder.absolute_stack(*scroll, std::string(id) + ".body", {state->extent.width, state->extent.height - state->authored.appearance.header_height});
            if (!body) return std::unexpected(body.error()); state->body = *body;
            auto edit_host = builder.content_host(*body, std::string(id) + ".editor", {1, 1});
            if (!edit_host) return std::unexpected(edit_host.error()); state->edit_host = *edit_host;
            if (auto frame = edit_host->set_frame({0, 0, 1, 1}); !frame) return std::unexpected(frame.error());
            if (auto hidden = edit_host->set_visible(false); !hidden) return std::unexpected(hidden.error());
            if (auto paint = header->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired"); return paint_header(state, view);
                }); !paint) return std::unexpected(paint.error());
            if (auto handler = header->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    try { auto result = NativeTable::header_input(state, event); if (!result) state->failure = result.error(); return result; }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table header input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto paint = body->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Table retired"); return paint_body(state, view);
                }); !paint) return std::unexpected(paint.error());
            if (auto handler = body->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state) return false;
                    try { auto result = NativeTable::input(state, event); if (!result) state->failure = result.error(); return result; }
                    catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Table input failed", std::current_exception()}; return std::unexpected(*state->failure); }
                }); !handler) return std::unexpected(handler.error());
            if (auto name = body->set_accessibility_semantics(ii::gui::AccessibilitySemantics{
                    .name=state->authored.appearance.accessibility_name}); !name) return std::unexpected(name.error());
            const auto on_viewport = [weak](const ii::gui::ScrollViewport& view) {
                if (auto state = weak.lock(); state && state->live) {
                    auto result = viewport(state, view); if (!result) { state->failure = result.error(); throw std::runtime_error(result.error().message); }
                    state->failure.reset();
                }
            };
            if (auto handler = scroll->set_scroll_handler(on_viewport); !handler) return std::unexpected(handler.error());
            if (auto arranged = layout(state); !arranged) return std::unexpected(arranged.error());
            if (auto enabled = root->set_enabled(state->authored.appearance.enabled); !enabled) return std::unexpected(enabled.error());
            if (auto layout_handler = root->set_layout_handler([weak](ii::gui::Rect bounds) { if (auto state = weak.lock()) {
                    if (state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; if (auto result = layout(state); !result) state->failure = result.error();
                }}); !layout_handler) return std::unexpected(layout_handler.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak, on_viewport] { if (auto state = weak.lock()) {
                    state->live = true; if (auto handler = state->scroll.set_scroll_handler(on_viewport); !handler) state->failure = handler.error();
                }}); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) { state->live = false; state->selecting = false; state->resize.reset(); if (state->editing) (void)state->editing->cancel(); state->editing.reset(); state->field.reset(); } }); !retired) return std::unexpected(retired.error());
            return NativeTable(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Table construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget scroll() const { return state_->scroll; }
    [[nodiscard]] ii::gui::Widget body() const { return state_->body; }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const TableViewportFrame>> frame() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->frame;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<void> begin_edit() const {
        try { return begin_edit(state_); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Opening a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> commit_edit() const {
        try { return commit_edit(state_); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Committing a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> copy_selection() const {
        try { return clipboard(state_, ClipboardOperation::copy); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Copying table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> cut_selection() const {
        try { return clipboard(state_, ClipboardOperation::cut); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Cutting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> paste_clipboard() const {
        try { return clipboard(state_, ClipboardOperation::paste); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Pasting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> paste_text(std::string text) const {
        try { return clipboard(state_, ClipboardOperation::paste, std::move(text)); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Pasting table cells failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> sort(std::string column, TableSortOrder direction) const {
        try { return sort(state_, std::move(column), direction); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Sorting table rows failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<std::uint64_t> apply_structure(TableStructureEdit edit) const {
        try { return structure(state_, std::move(edit)); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Changing table structure failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> cancel_edit() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (!state_->editing) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "No table cell is being edited");
        try { return clear_edit(state_, true); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Canceling a table edit failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
    [[nodiscard]] ii::core::Result<NativeInputField> editor() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Table requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (!state_->field) return ii::core::fail(ii::core::ErrorCode::not_found, "No table cell is being edited"); return *state_->field;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
