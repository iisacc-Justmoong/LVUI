#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
std::vector<ii::gui::ImageRun> images(const ii::gui::CanvasFrame& drawing) {
    auto result=drawing.images;for(const auto& layer:drawing.overlays)result.insert(result.end(),layer.images.begin(),layer.images.end());return result;
}
struct MutableIcons final:lvui::IconSource {
    std::string red,blue;mutable std::function<void()> on_resolve;mutable std::uint64_t version=1;
    [[nodiscard]] lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view name)const override {
        if(auto callback=std::exchange(on_resolve,{}))callback();
        if(name=="red"||name=="blue")return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{name=="red"?red:blue});
        return std::unexpected(lvui::IconError{lvui::IconErrorCode::not_found,"fixture icon missing"});
    }
    [[nodiscard]] std::uint64_t revision()const noexcept override{return version;}
};
}
int main() {
    using namespace ii::gui;
    std::vector<lvui::TableColumn> columns;
    for (int c = 0; c < 8; ++c) columns.push_back({.key="c" + std::to_string(c), .title="Column " + std::to_string(c), .width=80});
    std::vector<lvui::TableRow> rows;
    for (int r = 0; r < 1000; ++r) {
        lvui::TableRow row; row.key = "r" + std::to_string(r);
        for (int c = 0; c < 8; ++c) row.cells.push_back({.text=std::to_string(r) + "/" + std::to_string(c)});
        rows.push_back(std::move(row));
    }
    auto source = std::make_shared<lvui::TableMemory>(columns, rows);
    auto controller = std::make_shared<lvui::TableController>(source);
    lvui::native::TableInput input; input.controller = controller; input.appearance.width = 240; input.appearance.height = 124;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 160}));
    std::optional<lvui::native::NativeTable> table; std::vector<std::optional<lvui::TableSelection>> selections; bool remove_on_select = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto view = lvui::native::NativeTable::create(builder, *column, "table", input,
            {.selection_changed = [&](auto value) { selections.push_back(std::move(value)); if (remove_on_select) good(host.clear_content()); }});
        if (!view) return std::unexpected(view.error()); table = *view; return {};
    }));
    auto view = take(table->frame());
    require(view && view->content_width == 640 && view->content_height == 24000 && view->rows.size() <= 5 && view->columns.size() == 3,
            "Native table did not virtualize its visible cells");
    const auto initial_geometry = view->source; good(table->set_options(input));
    require(take(table->frame())->source == initial_geometry, "An unchanged reactive table input rebuilt its indexed geometry");
    auto drawing = take(tree->drawing_snapshot({300, 160}));
    require(has_text(drawing, "Column 0") && has_text(drawing, "0/0"), "Native table did not draw its header and cells");
    require(valid_canvas(drawing)&&images(drawing).size()==3,"Table did not draw the default leading icons for its visible headers");
    const std::string red="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\"><rect width=\"18\" height=\"18\" fill=\"#ff0000\"/></svg>";
    const std::string blue="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\"><rect width=\"18\" height=\"18\" fill=\"#0000ff\"/></svg>";
    auto catalog=lvui::IconCatalog::create({{"red",red},{"blue",blue}});require(bool(catalog),"Table icon fixture failed");
    auto styled=input;styled.header_icon_source=*catalog;styled.header_presentation.default_leading_icon="";
    styled.header_presentation.column_appearance={{.column="c0",.padding=3.,.leading_icon="red"},
                                                   {.column="c1",.trailing_icon="blue"}};
    good(table->set_options(styled));drawing=take(tree->drawing_snapshot({300,160}));
    auto runs=images(drawing);require(valid_canvas(drawing)&&runs.size()==2&&runs[0].image!=runs[1].image&&
        std::abs(runs[0].bounds.x-3)<.01&&take(source->snapshot())->revision==0,
        "Table did not apply per-column icons without changing document data");
    good(table->scroll().set_scroll_position(15,0));drawing=take(tree->drawing_snapshot({300,160}));runs=images(drawing);
    require(valid_canvas(drawing)&&!runs.empty()&&std::abs(runs[0].bounds.x)<.01&&std::abs(runs[0].bounds.width-6)<.01&&
        std::abs(runs[0].source.x-2.f/3.f)<.01,"Table header did not crop a scrolled icon correctly");
    good(table->scroll().set_scroll_position(240,0));require(images(take(tree->drawing_snapshot({300,160}))).empty(),
        "Offscreen table header icons remained visible");
    good(table->scroll().set_scroll_position(0,0));auto missing=styled;
    missing.header_presentation.column_appearance.push_back({.column="c7",.leading_icon="missing"});
    require(!table->set_options(missing)&&images(take(tree->drawing_snapshot({300,160}))).size()==2,
        "An invalid offscreen header icon replaced the table");
    auto pending=styled;pending.header_presentation.column_appearance.push_back({.column="future",.leading_icon="red"});
    good(table->set_options(pending));require(images(take(tree->drawing_snapshot({300,160}))).size()==2,
        "A pending presentation key changed existing columns");
    auto mutable_icons=std::make_shared<MutableIcons>();mutable_icons->red=red;mutable_icons->blue=blue;
    auto older=styled;older.header_icon_source=mutable_icons;auto newer=styled;newer.appearance.width=200;
    mutable_icons->on_resolve=[&]{good(table->set_options(newer));};good(table->set_options(older));
    require(take(table->root().snapshot()).size.width==200&&images(take(tree->drawing_snapshot({300,160}))).size()==2,
        "A reentrant icon source let an older table configuration overwrite the newer one");
    good(table->set_options(input));
    const auto send = [&](WidgetInputKind kind, double x, double y) { good(tree->input({.kind=kind, .x=x, .y=y, .buttons=1})); };
    send(WidgetInputKind::pointer_down, 40, 34); send(WidgetInputKind::pointer_move, 120, 54); send(WidgetInputKind::pointer_up, 120, 54);
    auto selected = take(controller->range());
    require(selected && selected->first_row == 0 && selected->last_row == 1 && selected->first_column == 0 && selected->last_column == 1 &&
            selections.size() == 2 && take(source->snapshot())->revision == 0, "Pointer selection did not preserve document state");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::right}));
    require(take(controller->selection())->current.column == "c2", "Table keyboard navigation did not advance the current cell");
    for (int i = 0; i < 5; ++i) good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::down}));
    require(take(controller->selection())->current.row == "r6" && take(table->scroll().snapshot()).scroll_offset > 0 &&
            take(source->snapshot())->revision == 0, "Table keyboard navigation did not reveal its row without editing");
    good(table->scroll().set_scroll_position(240, 20000)); view = take(table->frame());
    require(view->x == 240 && view->y == 20000 && view->rows.front().index > 800 && view->columns.front().index == 3 &&
            view->rows.size() <= 6 && view->columns.size() == 3, "Two-axis table scrolling materialized too many cells");
    const auto row_key = view->source->snapshot().rows[view->rows.front().index].key;
    auto changed = take(source->snapshot())->rows;
    auto changed_row = std::ranges::find_if(changed, [&](const auto& row) { return row.key == row_key; });
    changed_row->cells[3].text = "Updated cell";
    input.source_revision = std::int64_t(take(source->replace(columns, changed, 0))); good(table->set_options(input));
    drawing = take(tree->drawing_snapshot({300, 160}));
    require(has_text(drawing, "Updated cell") && take(table->frame())->anchor.row == row_key,
            "Reactive table source update lost its visible anchor or cell text");
    auto invalid = input; invalid.appearance.width = 0;
    require(!table->set_options(invalid) && has_text(take(tree->drawing_snapshot({300, 160})), "Updated cell"),
            "An invalid table configuration replaced the committed drawing");
    auto no_selection = input; no_selection.appearance.selection_enabled = false; good(table->set_options(no_selection));
    const auto disabled_count = selections.size(); send(WidgetInputKind::pointer_down, 10, 34); send(WidgetInputKind::pointer_up, 10, 34);
    require(selections.size() == disabled_count, "Disabled table selection still handled a click");
    good(table->set_options(input));
    auto stale_rows = take(source->snapshot())->rows;
    auto disabled = std::ranges::find_if(stale_rows, [&](const auto& row) { return row.key == row_key; });
    disabled->cells[3].enabled = false; (void)take(source->replace(columns, stale_rows, 1));
    const auto previous = selections.size(); send(WidgetInputKind::pointer_down, 10, 34); send(WidgetInputKind::pointer_up, 10, 34);
    require(selections.size() == previous && take(table->frame())->source_revision == 2,
            "A stale table action was delivered or failed to refresh its source");
    send(WidgetInputKind::pointer_down, 10, 34); send(WidgetInputKind::pointer_up, 10, 34);
    require(selections.size() == previous, "A disabled table cell was selectable");
    std::vector<lvui::TableColumn> wide; for (int i = 0; i < 100; ++i) wide.push_back({.key="c" + std::to_string(i), .width=32});
    std::vector<lvui::TableRow> dense;
    for (int i = 0; i < 100; ++i) { lvui::TableRow row; row.key = "r" + std::to_string(i); row.height = 18; row.cells.resize(wide.size()); dense.push_back(std::move(row)); }
    auto dense_source = std::make_shared<lvui::TableMemory>(wide, dense); auto too_large = input;
    too_large.controller = std::make_shared<lvui::TableController>(dense_source); too_large.appearance.width = 3200; too_large.appearance.height = 1824;
    require(!table->set_options(too_large) && take(table->root().snapshot()).size.width == 240 &&
            has_text(take(tree->drawing_snapshot({300, 160})), "Updated cell"),
            "An excessive table update replaced the committed view");
    auto candidate = host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto view = lvui::native::NativeTable::create(builder, {}, "dense", too_large);
        if (!view) return std::unexpected(view.error()); return {};
    });
    require(!candidate && table->root().snapshot() && has_text(take(tree->drawing_snapshot({300, 160})), "Updated cell"),
            "An excessive table view replaced the committed content");
    bool rejected = false;
    std::thread worker([&] { auto result = table->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Native table accepted a foreign-thread update");
    remove_on_select = true; send(WidgetInputKind::pointer_down, 90, 58);
    require(!table->set_options(input), "Table survived removal inside a selection callback"); tree->close();

    std::string long_text; for (int i = 0; i < 4000; ++i) long_text += "한";
    std::vector<lvui::TableRow> long_rows;
    for (int i = 0; i < 100; ++i) long_rows.push_back({.key="r" + std::to_string(i), .cells={{.text=long_text}}});
    auto long_source = std::make_shared<lvui::TableMemory>(std::vector<lvui::TableColumn>{{.key="text", .title="Text", .width=80}}, long_rows);
    lvui::native::TableInput long_input; long_input.controller = std::make_shared<lvui::TableController>(long_source);
    long_input.appearance.width = 80; long_input.appearance.height = 2424;
    auto long_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto long_host = take(long_tree->content_host({}, "host", {200, 2500})); std::optional<lvui::native::NativeTable> long_table;
    good(long_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto view = lvui::native::NativeTable::create(builder, *column, "long", long_input);
        if (!view) return std::unexpected(view.error()); long_table = *view; return {};
    }));
    (void)take(long_table->frame()); auto long_drawing = take(long_tree->drawing_snapshot({200, 2500}));
    std::size_t long_runs = 0; const auto check = [&](const auto& runs) { for (const auto& text : runs) if (text.text != "Text") {
        require(text.text.size() <= 8192 && lvui::valid_accessibility_text(text.text), "Long table text exceeded its budget or broke UTF-8"); ++long_runs;
    }};
    check(long_drawing.texts); for (const auto& layer : long_drawing.overlays) check(layer.texts);
    require(long_runs == 100 && take(long_source->snapshot())->rows.front().cells.front().text == long_text,
            "Drawing long cells lost visible runs or modified their full source text");
    auto stale=long_input;stale.header_icon_source=mutable_icons;stale.header_presentation.default_leading_icon="red";
    mutable_icons->on_resolve=[&]{auto snap=take(long_source->snapshot());auto changed=snap->columns;changed[0].title="New title";
        (void)take(long_source->replace(changed,snap->rows,snap->revision));};
    require(!long_table->set_options(stale)&&take(long_table->frame())->source_revision==0,
        "A source mutation during icon preparation published a stale table snapshot");
    long_input.source_revision=1;good(long_table->set_options(long_input));
    require(take(long_table->frame())->source_revision==1&&has_text(take(long_tree->drawing_snapshot({200,2500})),"New title"),
        "The table did not recover after a source change during icon preparation");
    ++mutable_icons->version;mutable_icons->on_resolve=[&]{good(long_host.clear_content());};
    good(long_table->set_options(stale));require(!long_table->root().snapshot(),"An icon callback revived a retired table");long_tree->close();
}
