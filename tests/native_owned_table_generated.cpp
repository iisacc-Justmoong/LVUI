#include "NativeOwnedTable.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
bool has_text(const ii::gui::CanvasFrame& drawing, std::string_view text) {
    for (const auto& item : drawing.texts) if (item.text == text) return true;
    for (const auto& layer : drawing.overlays) for (const auto& item : layer.texts) if (item.text == text) return true;
    return false;
}
}
int main() {
    using View = lvui_generated::NativeOwnedTable;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 140})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    (void)take(tree->snapshots());
    auto drawing = take(tree->drawing_snapshot({400, 140})); require(has_text(drawing, "First"), "Generated table did not paint its data");
    const auto send = [&](ii::gui::WidgetInputKind kind, double x, double y) { good(tree->input({.kind=kind, .x=x, .y=y, .buttons=1})); };
    send(ii::gui::WidgetInputKind::pointer_down, 10, 34); send(ii::gui::WidgetInputKind::pointer_move, 100, 60); send(ii::gui::WidgetInputKind::pointer_up, 100, 60);
    require(scope->backend_0.events == 2 && scope->backend_0.last && scope->backend_0.last->current.row == "r1" &&
            scope->backend_0.last->current.column == "b" && take(scope->backend_0.source->snapshot())->revision == 0,
            "Generated table selection changed source data or lost its callback");
    const auto revision = take(scope->backend_0.source->edit({.revision=0, .cells={{"r0", "a", "Updated"}}}));
    scope->backend_0.revision.set(std::int64_t(revision)); auto graph = scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty() && has_text(take(tree->drawing_snapshot({400, 140})), "Updated"),
            "Generated table did not apply the reactive source revision");
    scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty() && has_text(take(tree->drawing_snapshot({400, 140})), "Updated"),
            "An invalid reactive table width replaced the last good view");
    scope->backend_0.width.set(200); require(take(graph->failures()).empty(), "Generated table did not recover from invalid width");
    const auto previous = scope->backend_0.events; scope->backend_0.selection.set(false);
    send(ii::gui::WidgetInputKind::pointer_down, 10, 34); send(ii::gui::WidgetInputKind::pointer_up, 10, 34);
    require(scope->backend_0.events == previous, "Generated table ignored reactive selection disable");
    scope->backend_0.selection.set(true); good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::down}));
    require(scope->backend_0.last && scope->backend_0.last->current.row == "r2", "Generated table did not restore keyboard navigation");
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::select_all}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::text, .text="Done"}));
    require(scope->backend_0.edits == 1 && take(scope->backend_0.source->snapshot())->revision == 1,
            "Generated table draft edited the document or lost its callback");
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.submissions == 1 && take(scope->backend_0.source->snapshot())->revision == 2 &&
            take(scope->backend_0.source->snapshot())->rows[2].cells[1].text == "Done" && take(graph->failures()).empty(),
            "Generated table did not commit its editable cell");
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::select_all}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::text, .text="Mine"}));
    (void)take(scope->backend_0.source->edit({.revision=2, .cells={{"r2", "b", "External"}}}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.rejections == 1 && take(scope->backend_0.source->snapshot())->rows[2].cells[1].text == "External",
            "Generated table overwrote an external edit or lost its rejection callback");
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::escape}));
    send(ii::gui::WidgetInputKind::pointer_down, 10, 34); send(ii::gui::WidgetInputKind::pointer_up, 10, 34);
    send(ii::gui::WidgetInputKind::pointer_down, 10, 34); send(ii::gui::WidgetInputKind::pointer_up, 10, 34);
    auto content = std::make_shared<const ii::gui::ClipboardContent>(ii::gui::ClipboardContent{.text="X\tY\nZ\tW"});
    scope->backend_0.clipboard.set(false); good(tree->input({.kind=ii::gui::WidgetInputKind::text, .content=content}));
    require(take(scope->backend_0.source->snapshot())->revision == 3 && scope->backend_0.ranges == 0,
            "Generated table ignored clipboard disable");
    scope->backend_0.clipboard.set(true); good(tree->input({.kind=ii::gui::WidgetInputKind::text, .content=content}));
    require(take(scope->backend_0.source->snapshot())->revision == 4 && scope->backend_0.ranges == 1 &&
            scope->backend_0.last_range && scope->backend_0.last_range->last_row == 1 && scope->backend_0.last_range->last_column == 1 &&
            take(scope->backend_0.source->snapshot())->rows[1].cells[1].text == "W" && take(graph->failures()).empty(),
            "Generated table did not atomically paste or deliver its changed range");
    send(ii::gui::WidgetInputKind::pointer_down, 10, 10); send(ii::gui::WidgetInputKind::pointer_up, 10, 10);
    require(take(scope->backend_0.source->snapshot())->revision == 5 && scope->backend_0.sorts == 1 &&
            scope->backend_0.last_sort == lvui::TableSortOrder::ascending && take(scope->backend_0.source->snapshot())->rows[0].key == "r2",
            "Generated table did not sort its rows or deliver its sort callback");
    scope->backend_0.sorting.set(false);
    send(ii::gui::WidgetInputKind::pointer_down, 10, 10); send(ii::gui::WidgetInputKind::pointer_up, 10, 10);
    require(take(scope->backend_0.source->snapshot())->revision == 5 && scope->backend_0.sorts == 1,
            "Generated table ignored reactive sorting disable");
    send(ii::gui::WidgetInputKind::pointer_down, 78, 10); send(ii::gui::WidgetInputKind::pointer_move, 98, 10);
    require(take(scope->backend_0.source->snapshot())->revision == 5, "Generated column resize preview changed the source");
    send(ii::gui::WidgetInputKind::pointer_up, 98, 10);
    require(take(scope->backend_0.source->snapshot())->revision == 6 && take(scope->backend_0.source->snapshot())->columns[0].width == 100 &&
            scope->backend_0.structures == 1 && scope->backend_0.columns == 1,
            "Generated table did not commit its column resize");
    send(ii::gui::WidgetInputKind::pointer_down, 30, 46); send(ii::gui::WidgetInputKind::pointer_up, 30, 56);
    require(take(scope->backend_0.source->snapshot())->revision == 7 && take(scope->backend_0.source->snapshot())->rows[0].height == 34 &&
            scope->backend_0.structures == 2 && scope->backend_0.rows == 1,
            "Generated table did not commit its row resize");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired(), "Generated table retained its activation"); tree->close();
}
