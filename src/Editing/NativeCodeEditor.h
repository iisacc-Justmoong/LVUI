#pragma once

#include <LVUI/CodeEditorSession.h>
#include <LVUI/InputField.h>

#include <iiGUI/Desktop.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/TextLine.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

struct CodeEditorAppearance final {
    std::string font_family, accessibility_name = "Code editor";
    double width = 400, height = 128, horizontal_padding = 10, vertical_padding = 8;
    double font_size = 13, line_height = 20, tab_stop = 80, corner_radius = 8;
    bool enabled = true;
    InputFieldPalette palette;
    friend bool operator==(const CodeEditorAppearance&, const CodeEditorAppearance&) = default;
};
[[nodiscard]] inline StyleColor code_editor_color(ii::gui::WidgetColor value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}
struct CodeEditorInput final {
    std::shared_ptr<const CodeDocumentSource> source;
    std::shared_ptr<CodeDocumentEditor> editor;
    CodeEditorAppearance appearance;
};
struct NativeCodeEditorCallbacks final {
    std::function<void(const CodeDocumentChange&)> edited;
    std::function<void(CodeSelection)> selection_changed;
};
struct NativeCodeEditorClipboard final { std::function<ii::core::Result<void>(std::string)> write; };
struct NativeCodeEditorSnapshot final {
    std::uint64_t document_revision = 0;
    CodeSelection selection;
    std::optional<CodeDocumentRange> marked;
    double scroll_x = 0, scroll_y = 0;
    std::size_t first_column = 0;
};

// LVUI owns the document projection, selection, input session and drawing.
// iiGUI supplies replaceable host events and a generic text-line shaper.
class NativeCodeEditor final {
    struct State final {
        CodeEditorInput input;
        CodeEditorSession session;
        NativeCodeEditorCallbacks callbacks;
        NativeCodeEditorClipboard clipboard;
        ii::gui::Widget root;
        ii::gui::Size extent;
        std::string inherited_font;
        double scroll_x = 0, scroll_y = 0;
        std::size_t first_column = 0;
        std::optional<CodeEditorAppearance> pending_appearance;
        bool handling = false, dragging = false, dispatching = false, draining = false, pending_refresh = false;
        const std::thread::id owner = std::this_thread::get_id();
        State(CodeEditorInput value, CodeEditorSession active, NativeCodeEditorCallbacks events,
              NativeCodeEditorClipboard clip, std::string font)
            : input(std::move(value)), session(std::move(active)), callbacks(std::move(events)), clipboard(std::move(clip)),
              extent{input.appearance.width, input.appearance.height}, inherited_font(std::move(font)) {}
    };
    struct Area final { double x, y, width, height; };
    struct Shaped final { CodeViewportLine line; ii::gui::TextLine text; };
    struct Layout final { CodeViewportFrame frame; std::vector<Shaped> lines; };
    std::shared_ptr<State> state_;
    explicit NativeCodeEditor(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Error convert(const CodeDocumentError& value) {
        using E = CodeDocumentErrorCode; using C = ii::core::ErrorCode;
        const auto code = value.code == E::wrong_thread ? C::wrong_executor : value.code == E::busy ? C::busy :
            value.code == E::too_large || value.code == E::out_of_range || value.code == E::exhausted ? C::out_of_range : C::invalid_argument;
        return {code, value.message};
    }
    [[nodiscard]] static bool valid(const CodeEditorInput& input) noexcept {
        const auto& a = input.appearance;
        const auto range = [](double x, double lo, double hi) { return std::isfinite(x) && x >= lo && x <= hi; };
        const auto color = [&](StyleColor c) { return range(c.red,0,1) && range(c.green,0,1) && range(c.blue,0,1) && range(c.alpha,0,1); };
        return input.source && valid_accessibility_text(a.font_family) && a.font_family.size() <= 256 &&
            valid_accessibility_text(a.accessibility_name) && range(a.width,1,8192) && range(a.height,1,4096) &&
            range(a.horizontal_padding,0,1024) && range(a.vertical_padding,0,1024) && range(a.font_size,1,256) &&
            range(a.line_height,12,1024) && range(a.tab_stop,1,8192) && range(a.corner_radius,0,8192) &&
            color(a.palette.background) && color(a.palette.hover) && color(a.palette.text) && color(a.palette.selection) &&
            color(a.palette.caret) && color(a.palette.focus);
    }
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state, bool mutation = false) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Code editor requires its owner thread");
        if (mutation && state->handling) return ii::core::fail(ii::core::ErrorCode::busy, "Code editor is handling an input");
        auto alive = state->root.snapshot(); if (!alive) return std::unexpected(alive.error()); return {};
    }
    [[nodiscard]] static Area area(const State& state) noexcept {
        const auto& a = state.input.appearance;
        const auto x = std::min(a.horizontal_padding, std::max(0., state.extent.width / 2));
        const auto y = std::min(a.vertical_padding, std::max(0., state.extent.height / 2));
        return {x, y, std::max(0., state.extent.width - 2 * x), std::max(0., state.extent.height - 2 * y)};
    }
    [[nodiscard]] static std::size_t visible_count(const State& state) noexcept {
        return std::min<std::size_t>(512, std::size_t(std::ceil(area(state).height / state.input.appearance.line_height)) + 2);
    }
    [[nodiscard]] static std::size_t line_budget(const State& state) noexcept {
        return std::max<std::size_t>(2, std::min<std::size_t>(65536, (4 * 1024 * 1024) / std::max<std::size_t>(1, visible_count(state)) / 4));
    }
    [[nodiscard]] static ii::core::Result<CodeViewportFrame> viewport(const State& state) {
        auto view = state.session.view(); if (!view) return std::unexpected(convert(view.error()));
        if ((*view)->line_count() > 100000000) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Code view exceeds the native line limit");
        const auto first = std::min<std::size_t>((*view)->line_count(), std::size_t(state.scroll_y / state.input.appearance.line_height));
        auto frame = code_viewport(**view, {.first_line=first, .line_count=visible_count(state), .first_column=state.first_column,
            .line_units=line_budget(state), .maximum_bytes=4 * 1024 * 1024});
        if (!frame) return std::unexpected(convert(frame.error())); return std::move(*frame);
    }
    [[nodiscard]] static ii::core::Result<Layout> layout(const State& state) {
        auto frame = viewport(state); if (!frame) return std::unexpected(frame.error());
        Layout result; result.frame = std::move(*frame); result.lines.reserve(result.frame.lines.size());
        const auto& a = state.input.appearance;
        ii::gui::TextLineStyle style{.font_family=a.font_family.empty() ? state.inherited_font : a.font_family,
            .font_size=a.font_size, .line_height=a.line_height, .tab_stop_distance=a.tab_stop};
        for (const auto& line : result.frame.lines) {
            auto index = ii::gui::TextIndex::create(line.text); if (!index) return std::unexpected(index.error());
            auto shaped = ii::gui::TextLine::create(std::move(*index), style); if (!shaped) return std::unexpected(shaped.error());
            result.lines.push_back({line, std::move(*shaped)});
        }
        return result;
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value, bool enabled = true) noexcept {
        const auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(linear(value.red)),float(linear(value.green)),float(linear(value.blue)),float(value.alpha * (enabled ? 1 : .5))};
    }
    [[nodiscard]] static std::optional<ii::gui::Rect> clip(ii::gui::Rect rect, Area bounds) noexcept {
        const auto left = std::max(double(rect.x),bounds.x), top = std::max(double(rect.y),bounds.y);
        const auto right = std::min(double(rect.x + rect.width),bounds.x + bounds.width), bottom = std::min(double(rect.y + rect.height),bounds.y + bounds.height);
        if (right <= left || bottom <= top) return {}; return ii::gui::Rect{float(left),float(top),float(right-left),float(bottom-top)};
    }
    [[nodiscard]] static ii::core::Result<ii::gui::Rect> caret(const State& state, const Layout& shaped, std::size_t at) {
        auto source = state.session.view(); if (!source) return std::unexpected(convert(source.error()));
        auto line = code_line_at(**source, at); if (!line) return std::unexpected(convert(line.error()));
        const auto bounds = area(state); const auto& a = state.input.appearance;
        for (const auto& row : shaped.lines) if (row.line.line == *line) {
            const auto unit = std::clamp(at,row.line.visible_start,row.line.visible_end) - row.line.visible_start;
            auto x = row.text.caret(unit); if (!x) return std::unexpected(x.error());
            return ii::gui::Rect{float(bounds.x + *x - state.scroll_x),float(bounds.y + double(row.line.line) * a.line_height - state.scroll_y),1,float(a.line_height)};
        }
        return ii::gui::Rect{float(bounds.x),float(bounds.y),0,0};
    }
    [[nodiscard]] static ii::core::Result<std::size_t> position(const State& state, double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Code pointer position must be finite");
        auto shaped = layout(state); if (!shaped) return std::unexpected(shaped.error());
        if (shaped->lines.empty()) return std::size_t(0);
        const auto bounds = area(state); const auto height = state.input.appearance.line_height;
        const auto desired = std::max(0., std::floor((y - bounds.y + state.scroll_y) / height));
        const auto first = shaped->lines.front().line.line, last = shaped->lines.back().line.line;
        const auto line = std::clamp<std::size_t>(std::size_t(std::min(desired,double(last))),first,last);
        const auto& row = shaped->lines[line - first]; auto hit = row.text.hit_test(x - bounds.x + state.scroll_x);
        if (!hit) return std::unexpected(hit.error()); return row.line.visible_start + std::min(*hit,row.line.visible_end-row.line.visible_start);
    }
    [[nodiscard]] static ii::core::Result<ii::gui::TextInputState> text_state(const State& state) {
        auto source = state.session.view(); auto selection = state.session.selection(); auto marked = state.session.marked();
        if (!source) return std::unexpected(convert(source.error())); if (!selection) return std::unexpected(convert(selection.error()));
        if (!marked) return std::unexpected(convert(marked.error()));
        auto shaped = layout(state); if (!shaped) return std::unexpected(shaped.error());
        auto point = caret(state,*shaped,selection->caret); if (!point) return std::unexpected(point.error());
        const auto range = selection->range(); ii::gui::TextInputState result;
        result.length = (*source)->length(); result.selection = {range.location,range.length};
        if (*marked) result.marked = ii::gui::TextRange{(*marked)->location,(*marked)->length};
        result.caret = clip(*point,area(state)).value_or(ii::gui::Rect{float(area(state).x),float(area(state).y),0,0});
        result.read_only = !state.session.editable(); result.multiline = true; return result;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(const std::shared_ptr<State>& state,
            const ii::gui::WidgetPaintState& view) {
        try {
            auto drawing = std::make_shared<ii::gui::WidgetDrawing>(); const auto& a = state->input.appearance;
            const auto width = float(view.size.width), height = float(view.size.height);
            if (width <= 0 || height <= 0) return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            const auto fill = color(view.hovered ? a.palette.hover : a.palette.background,view.enabled);
            drawing->panels.push_back({{0,0,width,height},fill,fill,float(std::min({a.corner_radius,double(width)/2,double(height)/2}))});
            if (view.focused) { const auto edge = color(a.palette.focus,view.enabled); drawing->panels.push_back({{0,0,width,height},edge,edge,
                float(std::min({a.corner_radius,double(width)/2,double(height)/2})),1}); }
            const auto bounds = area(*state); if (bounds.width <= 0 || bounds.height <= 0)
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            auto shaped = layout(*state); if (!shaped) return std::unexpected(shaped.error());
            auto selection = state->session.selection(); auto marked = state->session.marked();
            if (!selection) return std::unexpected(convert(selection.error())); if (!marked) return std::unexpected(convert(marked.error()));
            const auto draw_range = [&](CodeDocumentRange range, StyleColor fill_color, bool underline) -> ii::core::Result<void> {
                const auto end = range.location + range.length;
                for (const auto& row : shaped->lines) {
                    const auto first = std::max(range.location,row.line.visible_start), last = std::min(end,row.line.visible_end);
                    if (first >= last) continue;
                    auto spans = row.text.selection(first-row.line.visible_start,last-row.line.visible_start); if (!spans) return std::unexpected(spans.error());
                    const auto y = bounds.y + double(row.line.line) * a.line_height - state->scroll_y;
                    for (const auto& span : *spans) {
                        ii::gui::Rect rect{float(bounds.x + span.x - state->scroll_x),float(y + (underline ? a.line_height-1 : 0)),
                            float(span.width),float(underline ? 1 : a.line_height)};
                        if (auto clipped = clip(rect,bounds)) { const auto ink = color(fill_color,view.enabled); drawing->panels.push_back({*clipped,ink,ink,0}); }
                    }
                }
                return {};
            };
            if (view.focused && selection->range().length) if (auto r=draw_range(selection->range(),a.palette.selection,false); !r) return std::unexpected(r.error());
            if (view.focused && *marked) if (auto r=draw_range(**marked,a.palette.focus,true); !r) return std::unexpected(r.error());
            if (view.focused && state->session.editable()) {
                auto point = caret(*state,*shaped,selection->caret); if (!point) return std::unexpected(point.error());
                if (auto rect = clip(*point,bounds)) { const auto ink=color(a.palette.caret,view.enabled); drawing->panels.push_back({*rect,ink,ink,0}); }
            }
            const auto ink = color(a.palette.text,view.enabled);
            for (const auto& row : shaped->lines) {
                if (row.line.text.empty()) continue;
                const auto y = bounds.y + double(row.line.line) * a.line_height - state->scroll_y;
                const auto top = std::max(y,bounds.y), bottom = std::min(y+a.line_height,bounds.y+bounds.height);
                if (bottom <= top) continue;
                auto bitmap = row.text.render(state->scroll_x,bounds.width,ink,view.backing_scale); if (!bitmap) return std::unexpected(bitmap.error());
                auto image = ii::gui::rgba_image(bitmap->width,bitmap->height,std::move(bitmap->rgba)); if (!image) return std::unexpected(image.error());
                ii::gui::ImageRun run; run.bounds={float(bounds.x),float(top),float(bounds.width),float(bottom-top)}; run.image=std::move(*image);
                run.source={0,float((top-y)/a.line_height),1,float((bottom-top)/a.line_height)}; drawing->images.push_back(std::move(run));
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Code editor paint failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<void> reveal(State& state) {
        auto view = state.session.view(); auto selection = state.session.selection();
        if (!view) return std::unexpected(convert(view.error())); if (!selection) return std::unexpected(convert(selection.error()));
        auto line = code_line_at(**view,selection->caret); if (!line) return std::unexpected(convert(line.error()));
        auto bounds = code_line_bounds(**view,*line); if (!bounds) return std::unexpected(convert(bounds.error()));
        const auto column = std::min(selection->caret,bounds->end) - bounds->start, budget = line_budget(state);
        if (column < state.first_column) { state.first_column = column; state.scroll_x = 0; }
        else if (column - state.first_column >= budget) { state.first_column = column - budget / 2; state.scroll_x = 0; }
        const auto row_y = double(*line) * state.input.appearance.line_height, height = area(state).height;
        if (row_y < state.scroll_y) state.scroll_y = row_y;
        else if (row_y + state.input.appearance.line_height > state.scroll_y + height)
            state.scroll_y = std::max(0.,row_y + state.input.appearance.line_height - height);
        auto shaped = layout(state); if (!shaped) return std::unexpected(shaped.error());
        auto point = caret(state,*shaped,selection->caret); if (!point) return std::unexpected(point.error());
        const auto a = area(state);
        if (point->x < a.x) state.scroll_x = std::max(0.,state.scroll_x + point->x - a.x);
        else if (point->x + point->width > a.x + a.width) state.scroll_x += point->x + point->width - (a.x + a.width);
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> publish(const std::shared_ptr<State>& state, CodeSelection before,
            bool text_changed, const std::vector<CodeDocumentChange>& edits = {}) {
        if (auto visible = reveal(*state); !visible) return visible;
        if (auto r=state->root.notify_text_input_changed(text_changed); !r) return r;
        if (auto r=state->root.request_repaint(); !r) return r;
        auto after = state->session.selection(); if (!after) return std::unexpected(convert(after.error()));
        {
            state->dispatching=true; struct Reset final { bool& value; ~Reset(){value=false;} } reset{state->dispatching};
            for (const auto& edit : edits) {
                if (state->callbacks.edited) { auto callback = state->callbacks.edited; callback(edit); }
                if (!state->root.snapshot()) return {};
            }
            if (*after != before && state->callbacks.selection_changed) { auto callback = state->callbacks.selection_changed; callback(*after); }
        }
        if (state->draining || (!state->pending_appearance && !state->pending_refresh) || !state->root.snapshot()) return {};
        const bool handling=state->handling; state->handling=false; state->draining=true;
        struct DrainReset final { State& state; bool handling; ~DrainReset(){state.handling=handling;state.draining=false;} } reset{*state,handling};
        for (unsigned pass=0; pass<8 && (state->pending_appearance || state->pending_refresh); ++pass) {
            auto appearance=std::exchange(state->pending_appearance,std::nullopt);
            const bool refresh=std::exchange(state->pending_refresh,false);
            if (appearance) if (auto r=NativeCodeEditor(state).set_appearance(std::move(*appearance)); !r) return r;
            if (refresh) if (auto r=NativeCodeEditor(state).sync_source(); !r) return r;
            if (!state->root.snapshot()) return {};
        }
        if (state->pending_appearance || state->pending_refresh) return ii::core::fail(ii::core::ErrorCode::busy,"Code editor updates did not settle");
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state, const ii::gui::WidgetInput& event) {
        using Kind=ii::gui::WidgetInputKind; using Key=ii::gui::WidgetKey;
        if (auto checked=ready(state,true); !checked) return std::unexpected(checked.error());
        if (!state->input.appearance.enabled) return false;
        try {
            state->handling = true; struct Reset final { bool& value; ~Reset() { value=false; } } reset{state->handling};
            auto before = state->session.selection(); if (!before) return std::unexpected(convert(before.error()));
            std::vector<CodeDocumentChange> edits; bool changed = false, text_changed = false;
            const auto commit = [&]() -> ii::core::Result<void> {
                auto result=state->session.commit_composition(); if (!result) return std::unexpected(convert(result.error()));
                if (*result) { if ((**result).revision != (**result).before_revision) edits.push_back(**result);
                    changed=true; text_changed=true; } return {};
            };
            if (event.kind == Kind::composition) {
                if (!state->session.editable()) return true;
                const auto replacement = event.replacement_range ? std::optional<CodeDocumentRange>{{event.replacement_range->location,event.replacement_range->length}} : std::nullopt;
                const auto selected = event.composition_selection ? std::optional<CodeDocumentRange>{{event.composition_selection->location,event.composition_selection->length}} : std::nullopt;
                auto result=state->session.set_composition(event.text,replacement,selected); if (!result) return std::unexpected(convert(result.error()));
                changed=*result; text_changed=changed;
            } else if (event.kind == Kind::commit_composition) {
                if (auto result=commit(); !result) return std::unexpected(result.error());
            } else if (event.kind == Kind::text) {
                if (!state->session.editable()) return true;
                if (event.content && event.content->text.empty()) return true;
                const auto replacement = event.replacement_range ? std::optional<CodeDocumentRange>{{event.replacement_range->location,event.replacement_range->length}} : std::nullopt;
                const auto& text = event.content ? event.content->text : event.text;
                auto marked=state->session.marked(); if (!marked) return std::unexpected(convert(marked.error()));
                auto result=state->session.insert(text,replacement); if (!result) return std::unexpected(convert(result.error()));
                auto after=state->session.selection(); if (!after) return std::unexpected(convert(after.error()));
                const bool edited=result->revision!=result->before_revision;
                if (edited) edits.push_back(*result); changed=edited||marked->has_value()||*after!=*before; text_changed=edited||marked->has_value();
            } else if (event.kind == Kind::pointer_down || (state->dragging && (event.kind == Kind::pointer_move || event.kind == Kind::pointer_up))) {
                if (auto result=commit(); !result) return std::unexpected(result.error());
                if (!event.local_position) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Code editor needs local pointer coordinates");
                if (event.kind == Kind::pointer_down) if (auto focus=state->root.focus(); !focus) return std::unexpected(focus.error());
                auto hit=position(*state,event.local_position->x,event.local_position->y); if (!hit) return std::unexpected(hit.error());
                auto result=state->session.set_selection(event.kind==Kind::pointer_down && !event.shift ? *hit : before->anchor,*hit);
                if (!result) return std::unexpected(convert(result.error())); changed=changed||*result; state->dragging=event.kind!=Kind::pointer_up;
            } else if (event.kind == Kind::focus_lost) { state->dragging=false; return false;
            } else if (event.kind == Kind::scroll) {
                auto view=state->session.view(); if (!view) return std::unexpected(convert(view.error()));
                if ((*view)->line_count()>100000000) return ii::core::fail(ii::core::ErrorCode::out_of_range,"Code view exceeds the native line limit");
                const auto maximum=std::max(0.,double((*view)->line_count()) * state->input.appearance.line_height-area(*state).height);
                state->scroll_y=std::clamp(state->scroll_y+event.scroll_y,0.,maximum);
                state->scroll_x=std::clamp(state->scroll_x+event.scroll_x,0.,1e7);
                if (auto r=state->root.notify_text_input_changed(false); !r) return std::unexpected(r.error());
                if (auto r=state->root.request_repaint(); !r) return std::unexpected(r.error()); return true;
            } else if (event.kind == Kind::key_down) {
                if (auto result=commit(); !result) return std::unexpected(result.error());
                std::optional<CodeMove> move;
                if (event.key==Key::left) move=CodeMove::previous_scalar; else if (event.key==Key::right) move=CodeMove::next_scalar;
                else if (event.key==Key::up) move=CodeMove::previous_line; else if (event.key==Key::down) move=CodeMove::next_line;
                else if (event.key==Key::home) move=event.control||event.meta?CodeMove::document_start:CodeMove::line_start;
                else if (event.key==Key::end) move=event.control||event.meta?CodeMove::document_end:CodeMove::line_end;
                if (move) { auto result=state->session.move(*move,event.shift); if (!result) return std::unexpected(convert(result.error())); changed=changed||*result; }
                else if (event.key==Key::select_all) {
                    auto view=state->session.view(); if (!view) return std::unexpected(convert(view.error()));
                    auto result=state->session.set_selection(0,(*view)->length()); if (!result) return std::unexpected(convert(result.error())); changed=changed||*result;
                } else if (event.key==Key::backspace || event.key==Key::delete_forward) {
                    if (state->session.editable()) { auto result=state->session.erase(event.key==Key::backspace?CodeErase::previous_scalar:CodeErase::next_scalar);
                        if (!result) return std::unexpected(convert(result.error())); if (*result) { edits.push_back(**result); changed=true; text_changed=true; } }
                } else if (event.key==Key::enter && state->session.editable()) {
                    auto result=state->session.insert("\n"); if (!result) return std::unexpected(convert(result.error()));
                    if (result->revision!=result->before_revision) edits.push_back(*result); changed=true; text_changed=true;
                } else if (event.key==Key::copy || event.key==Key::cut) {
                    auto selection=state->session.selection(); auto view=state->session.view(); if (!selection) return std::unexpected(convert(selection.error()));
                    if (!view) return std::unexpected(convert(view.error())); const auto range=selection->range();
                    if (range.length) {
                        auto text=(*view)->read(range,4*1024*1024); if (!text) return std::unexpected(convert(text.error()));
                        auto copied=state->clipboard.write?state->clipboard.write(std::move(*text)):ii::gui::write_clipboard({.text=std::move(*text)});
                        if (!copied) return std::unexpected(copied.error());
                        if (event.key==Key::cut && state->session.editable()) { auto result=state->session.insert({});
                            if (!result) return std::unexpected(convert(result.error()));
                            if (result->revision!=result->before_revision) edits.push_back(*result); changed=true; text_changed=true; }
                    }
                } else { if (changed) { if (auto r=publish(state,*before,text_changed,edits); !r) return std::unexpected(r.error()); } return false; }
            } else return false;
            if (changed) if (auto result=publish(state,*before,text_changed,edits); !result) return std::unexpected(result.error());
            return true;
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Code editor input failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<NativeCodeEditor> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, CodeEditorInput input, NativeCodeEditorCallbacks callbacks = {}, NativeCodeEditorClipboard clipboard = {}) {
        if (!valid(input)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid code editor input");
        try {
            auto session=CodeEditorSession::create(input.source,input.editor); if (!session) return std::unexpected(convert(session.error()));
            auto appearance=builder.appearance(); if (!appearance) return std::unexpected(appearance.error());
            auto state=std::make_shared<State>(std::move(input),std::move(*session),std::move(callbacks),std::move(clipboard),appearance->font_family);
            auto root=builder.absolute_stack(parent,id,state->extent); if (!root) return std::unexpected(root.error()); state->root=*root;
            std::weak_ptr<State> weak=state;
            if (auto r=root->set_input_handler([state](const auto& event){return NativeCodeEditor::input(state,event);}); !r) return std::unexpected(r.error());
            auto provider=std::make_shared<ii::gui::TextInputProvider>();
            provider->state=[weak]() -> ii::core::Result<ii::gui::TextInputState> { auto state=weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed,"Code editor retired"); return text_state(*state); };
            provider->read=[weak](ii::gui::TextRange range) -> ii::core::Result<ii::gui::TextContent> { auto state=weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed,"Code editor retired");
                auto view=state->session.view(); if (!view) return std::unexpected(convert(view.error())); auto text=(*view)->read({range.location,range.length},4*1024*1024);
                if (!text) return std::unexpected(convert(text.error())); return ii::gui::TextContent{range,std::move(*text)}; };
            provider->position=[weak](double x,double y) -> ii::core::Result<std::size_t> { auto state=weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed,"Code editor retired"); return position(*state,x,y); };
            provider->focus_changed=[weak](bool focused) -> ii::core::Result<void> { auto state=weak.lock(); if (!state || focused) return {};
                auto marked=state->session.marked(); if (!marked) return std::unexpected(convert(marked.error())); if (!*marked) return {};
                auto result=NativeCodeEditor::input(state,ii::gui::WidgetInput{.kind=ii::gui::WidgetInputKind::commit_composition}); if (!result) return std::unexpected(result.error()); return {}; };
            if (auto r=root->set_text_input_provider(std::move(provider)); !r) return std::unexpected(r.error());
            if (auto r=root->set_accessibility_name(state->input.appearance.accessibility_name); !r) return std::unexpected(r.error());
            if (auto r=root->set_enabled(state->input.appearance.enabled); !r) return std::unexpected(r.error());
            if (auto r=root->set_layout_handler([weak](ii::gui::Rect rect){ if (auto state=weak.lock()) state->extent={rect.width,rect.height}; }); !r) return std::unexpected(r.error());
            if (auto r=root->set_paint_handler([state](const auto& value){return paint(state,value);}); !r) return std::unexpected(r.error());
            return NativeCodeEditor(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Code editor construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<NativeCodeEditorSnapshot> snapshot() const {
        if (auto r=ready(state_); !r) return std::unexpected(r.error());
        auto view=state_->session.view(); auto selected=state_->session.selection(); auto marked=state_->session.marked();
        if (!view) return std::unexpected(convert(view.error())); if (!selected) return std::unexpected(convert(selected.error()));
        if (!marked) return std::unexpected(convert(marked.error()));
        return NativeCodeEditorSnapshot{(*view)->revision(),*selected,*marked,state_->scroll_x,state_->scroll_y,state_->first_column};
    }
    [[nodiscard]] ii::core::Result<CodeViewportFrame> frame() const {
        if (auto r=ready(state_); !r) return std::unexpected(r.error()); return viewport(*state_);
    }
    [[nodiscard]] ii::core::Result<CodeEditorAppearance> appearance() const {
        if (auto r=ready(state_); !r) return std::unexpected(r.error()); return state_->input.appearance;
    }
    [[nodiscard]] ii::core::Result<void> set_appearance(CodeEditorAppearance value) const {
        if (auto r=ready(state_); !r) return r;
        auto candidate=state_->input; candidate.appearance=value;
        if (!valid(candidate)) return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid code editor appearance");
        if (state_->handling) {
            if (!state_->dispatching) return ii::core::fail(ii::core::ErrorCode::busy,"Code editor is handling an input");
            state_->pending_appearance=std::move(value); return {};
        }
        if (value == state_->input.appearance) return {};
        auto marked=state_->session.marked(); if (!marked) return std::unexpected(convert(marked.error()));
        if (*marked && !value.enabled) return ii::core::fail(ii::core::ErrorCode::busy,"Commit or cancel composition before disabling the code editor");
        try {
            state_->handling=true; struct Reset final { bool& value; ~Reset(){value=false;} } reset{state_->handling};
            const auto previous=state_->input.appearance; const auto old_extent=state_->extent;
            state_->input.appearance=value;
            const auto rollback=[&] { state_->input.appearance=previous; state_->extent=old_extent;
                (void)state_->root.set_extent({previous.width,previous.height}); (void)state_->root.set_enabled(previous.enabled);
                (void)state_->root.set_accessibility_name(previous.accessibility_name); };
            if (auto r=state_->root.set_extent({value.width,value.height}); !r) { rollback(); return r; }
            if (auto r=state_->root.set_enabled(value.enabled); !r) { rollback(); return r; }
            if (auto r=state_->root.set_accessibility_name(value.accessibility_name); !r) { rollback(); return r; }
            if (auto r=reveal(*state_); !r) { rollback(); return r; }
            if (auto r=state_->root.notify_text_input_changed(false); !r) { rollback(); return r; }
            if (auto r=state_->root.request_repaint(); !r) { rollback(); return r; } return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Code editor appearance failed",std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> set_selection(std::size_t anchor, std::size_t caret) const {
        if (auto r=ready(state_,true); !r) return r;
        try { state_->handling=true; struct Reset final { bool& value; ~Reset(){value=false;} } reset{state_->handling};
            auto before=state_->session.selection(); if (!before) return std::unexpected(convert(before.error()));
            auto result=state_->session.set_selection(anchor,caret); if (!result) return std::unexpected(convert(result.error()));
            if (*result) return publish(state_,*before,false); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Code selection failed",std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> insert(std::string text) const {
        if (auto r=ready(state_,true); !r) return r;
        try { state_->handling=true; struct Reset final { bool& value; ~Reset(){value=false;} } reset{state_->handling};
            auto before=state_->session.selection(); if (!before) return std::unexpected(convert(before.error()));
            auto marked=state_->session.marked(); if (!marked) return std::unexpected(convert(marked.error()));
            auto result=state_->session.insert(std::move(text)); if (!result) return std::unexpected(convert(result.error()));
            auto after=state_->session.selection(); if (!after) return std::unexpected(convert(after.error()));
            const bool edited=result->revision!=result->before_revision;
            if (!edited && !marked->has_value() && *after==*before) return {};
            return publish(state_,*before,edited||marked->has_value(),edited?std::vector<CodeDocumentChange>{*result}:std::vector<CodeDocumentChange>{});
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Code insertion failed",std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> refresh() const {
        if (auto r=ready(state_,true); !r) return r;
        try { state_->handling=true; struct Reset final { bool& value; ~Reset(){value=false;} } reset{state_->handling};
            auto before=state_->session.selection(); if (!before) return std::unexpected(convert(before.error()));
            auto result=state_->session.refresh(); if (!result) return std::unexpected(convert(result.error()));
            if (*result) return publish(state_,*before,true); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Code refresh failed",std::current_exception()); }
    }
    // An observer may echo the revision emitted by this view during its edit
    // callback; that confirmation is a no-op even while the callback is active.
    [[nodiscard]] ii::core::Result<void> sync_source() const {
        if (auto r=ready(state_); !r) return r;
        auto latest=state_->input.source->snapshot(); auto current=state_->session.view();
        if (!latest) return std::unexpected(convert(latest.error()));
        if (!current) return std::unexpected(convert(current.error()));
        if (!*latest) return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Code source returned no snapshot");
        if ((*latest)->revision()==(*current)->revision()) return {};
        if (state_->handling && state_->dispatching) { state_->pending_refresh=true; return {}; }
        return refresh();
    }
    [[nodiscard]] ii::core::Result<void> scroll_to_line(std::size_t line) const {
        if (auto r=ready(state_,true); !r) return r;
        auto view=state_->session.view(); if (!view) return std::unexpected(convert(view.error()));
        if ((*view)->line_count()>100000000) return ii::core::fail(ii::core::ErrorCode::out_of_range,"Code view exceeds the native line limit");
        if (line>=(*view)->line_count()) return ii::core::fail(ii::core::ErrorCode::out_of_range,"Code line does not exist");
        const auto height=state_->input.appearance.line_height, maximum=std::max(0.,double((*view)->line_count())*height-area(*state_).height);
        state_->scroll_y=std::min(double(line)*height,maximum);
        if (auto r=state_->root.notify_text_input_changed(false); !r) return r; return state_->root.request_repaint();
    }
};

} // namespace lvui::native
