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
class Factory final : public lvui::TableStructureFactory {
public:
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot& value, std::size_t) const override {
        return lvui::TableRow{.key="custom-row", .cells=std::vector<lvui::TableCell>(value.columns.size(), {.text="row"})};
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot& value, std::size_t) const override {
        return lvui::TableColumnInsertion{.column={.key="custom-column"}, .cells=std::vector<lvui::TableCell>(value.rows.size(), {.text="column"})};
    }
};
class ReentrantFactory final : public lvui::TableStructureFactory {
public:
    std::function<void()> change;
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot& value, std::size_t index) const override {
        auto row = lvui::TableDefaultStructureFactory{}.make_row(value, index); if (change) change(); return row;
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot& value, std::size_t index) const override {
        return lvui::TableDefaultStructureFactory{}.make_column(value, index);
    }
};
}
int main() {
    using namespace ii::gui;
    auto source = std::make_shared<lvui::TableMemory>(std::vector<lvui::TableColumn>{{.key="a", .width=80}, {.key="b", .width=80}},
        std::vector<lvui::TableRow>{{.key="r0", .cells={{.text="A"}, {.text="B"}}}, {.key="r1", .cells={{.text="C"}, {.text="D"}}}});
    auto editor = std::make_shared<lvui::TableMemoryStructureEditor>(source);
    lvui::native::TableInput input; input.controller = std::make_shared<lvui::TableController>(source); input.structure_editor = editor;
    input.structure_factory = std::make_shared<Factory>(); input.structure_controls_visible = true;
    input.appearance.width = 220; input.appearance.height = 160;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {500, 300})); std::optional<lvui::native::NativeTable> table;
    int changes = 0; bool retire_on_clear = false;
    std::vector<std::optional<lvui::TableSelection>> selections;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "stack", {.spacing=0, .alignment=StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        auto value = lvui::native::NativeTable::create(builder, *stack, "table", input,
            {.selection_changed=[&](auto value) { selections.push_back(std::move(value));
                 if (retire_on_clear && !selections.back()) good(host.clear_content()); },
             .structure_changed=[&](const auto&) { ++changes; }});
        if (!value) return std::unexpected(value.error()); table = *value; return {};
    }));
    const auto find = [&](std::string_view suffix) {
        const auto nodes = take(tree->snapshots()); const auto node = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
        if (node == nodes.end()) throw std::runtime_error("Table structure control was not mounted"); return take(tree->find(node->id));
    };
    auto rows = find(".row-controls"), columns = find(".column-controls");
    auto menu = find(".structure-menu"), delete_row = find(".delete-row"), delete_column = find(".delete-column");
    auto row_bounds = take(rows.snapshot()).bounds, column_bounds = take(columns.snapshot()).bounds;
    require(row_bounds.width == 20 && column_bounds.height == 20 && take(table->frame())->width == 200,
            "Structure gutters did not reserve viewport space");
    const auto send = [&](WidgetInputKind kind, double x, double y) { return tree->input({.kind=kind, .x=x, .y=y, .buttons=1}); };
    const auto click = [&](double x, double y) { good(send(WidgetInputKind::pointer_down, x, y)); good(send(WidgetInputKind::pointer_up, x, y)); };
    const auto drawing = take(tree->drawing_snapshot({500,300}));
    const auto root_bounds = take(table->root().snapshot()).bounds;
    const auto has_panel = [&](double x, double y, double width, double height) {
        const auto matches = [&](const auto& panels) { return std::ranges::any_of(panels, [&](const auto& panel) {
            const auto& rect = panel.bounds; return std::abs(rect.x-x)<.1 && std::abs(rect.y-y)<.1 &&
                std::abs(rect.width-width)<.1 && std::abs(rect.height-height)<.1;
        }); };
        if (matches(drawing.panels)) return true;
        return std::ranges::any_of(drawing.overlays, [&](const auto& layer) { return matches(layer.panels); });
    };
    require(has_panel(root_bounds.x,root_bounds.y,root_bounds.width,root_bounds.height),
            "Table background did not cover the gutter corners");
    require(has_panel(row_bounds.x+6,row_bounds.y+12,8,1) && has_panel(row_bounds.x+10,row_bounds.y+8,1,8) &&
            has_panel(column_bounds.x+36,column_bounds.y+10,8,1) && has_panel(column_bounds.x+40,column_bounds.y+6,1,8),
            "Structure controls did not draw both visible insertion symbols");
    input.appearance.palette.background = {.5, .5, .5, 1}; good(table->set_options(input));
    const auto recolored = take(tree->drawing_snapshot({500,300}));
    const auto recolored_panel = [&](const auto& panels) { return std::ranges::any_of(panels, [&](const auto& panel) {
        return std::abs(panel.bounds.x-root_bounds.x)<.1 && std::abs(panel.bounds.y-root_bounds.y)<.1 &&
            std::abs(panel.bounds.width-root_bounds.width)<.1 && std::abs(panel.bounds.height-root_bounds.height)<.1 &&
            std::abs(panel.top.red-.214041f)<.001;
    }); };
    require(recolored_panel(recolored.panels) || std::ranges::any_of(recolored.overlays, [&](const auto& layer) { return recolored_panel(layer.panels); }),
            "A reactive table background change did not repaint the gutter corners");
    click(row_bounds.x + 10, row_bounds.y + 12);
    auto value = take(source->snapshot());
    require(value->revision == 1 && value->rows[1].key == "custom-row" && value->rows[1].cells[1].text == "row" && changes == 1,
            "Row control failed to insert a domain row after the clicked row");
    click(column_bounds.x + 40, column_bounds.y + 10);
    value = take(source->snapshot());
    require(value->revision == 2 && value->columns[1].key == "custom-column" && value->rows[2].cells[1].text == "column" && changes == 2,
            "Column control failed to insert a domain column after the clicked column");
    auto body = take(table->body().snapshot());
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body.bounds.x+40, .y=body.bounds.y+12, .target=body.id}));
    require(take(menu.snapshot()).opened && take(input.controller->selection())->current.row == "r0",
            "Context request did not select and open on the target cell");
    input.appearance.palette.current = lvui::figma_defaults::accent_purple_default;
    good(table->set_options(input));
    require(take(menu.snapshot()).opened && take(input.controller->selection())->current.row == "r0",
            "A palette change dismissed the table context menu or changed selection");
    good(delete_row.activate()); value = take(source->snapshot());
    require(value->revision == 3 && value->rows[0].key == "custom-row" && changes == 3 &&
            selections.size() == 2 && selections[0] && !selections[1],
            "Context deletion did not remove the targeted row or publish the cleared selection");
    input.source_revision = 3; input.select_on_context_click = false; input.delete_context_menu_enabled = false;
    good(table->set_options(input)); body = take(table->body().snapshot());
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body.bounds.x+40, .y=body.bounds.y+12, .target=body.id}));
    require(!take(menu.snapshot()).opened && !take(input.controller->selection()), "Disabled context menu opened or selected a cell");
    input.delete_context_menu_enabled = true; good(table->set_options(input));
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body.bounds.x+40, .y=body.bounds.y+12, .target=body.id}));
    require(take(menu.snapshot()).opened && !take(input.controller->selection()), "Context menu ignored selection-on-click disable");
    (void)take(editor->apply({.revision=3, .operations={lvui::TableResizeRows{{{"r1", 30}}}}}));
    good(delete_row.activate());
    require(take(source->snapshot())->revision == 4 && take(source->snapshot())->rows.size() == 2 && changes == 3,
            "Stale context action removed a row after an external change");
    input.source_revision = 4; input.select_on_context_click = true; good(table->set_options(input));
    good(send(WidgetInputKind::pointer_down, row_bounds.x+10, row_bounds.y+12));
    good(send(WidgetInputKind::pointer_up, row_bounds.x-5, row_bounds.y+12));
    require(take(source->snapshot())->revision == 4 && changes == 3, "Releasing outside the insertion control changed the model");
    good(send(WidgetInputKind::pointer_down, row_bounds.x+10, row_bounds.y+12));
    (void)take(editor->apply({.revision=4, .operations={lvui::TableResizeRows{{{"r1", 36}}}}}));
    require(!send(WidgetInputKind::pointer_up, row_bounds.x+10, row_bounds.y+12) && take(source->snapshot())->revision == 5 && changes == 3,
            "A stale insertion overwrote an external change");
    input.source_revision = 5; good(table->set_options(input));
    input.appearance.enabled = false; good(table->set_options(input));
    click(row_bounds.x+10,row_bounds.y+12);
    require(take(source->snapshot())->revision == 5 && changes == 3,"Disabled structure controls changed the model");
    input.appearance.enabled = true; input.add_row_controls_visible = false; good(table->set_options(input));
    require(!take(rows.snapshot()).visible && take(columns.snapshot()).visible && take(table->frame())->width == 220,
            "Independently hiding row controls did not restore table width");
    input.add_row_controls_visible = true; input.add_column_controls_visible = false; good(table->set_options(input));
    require(take(rows.snapshot()).visible && !take(columns.snapshot()).visible && take(table->frame())->width == 200,
            "Independently hiding column controls changed the table width");
    input.add_column_controls_visible = true;
    (void)take(editor->apply({.revision=5, .operations={lvui::TableRemoveColumns{{"custom-column", "b"}}}}));
    input.source_revision = 6; good(table->set_options(input)); body = take(table->body().snapshot());
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body.bounds.x+40, .y=body.bounds.y+12, .target=body.id}));
    require(take(menu.snapshot()).opened && !take(delete_column.snapshot()).enabled, "Last-column deletion was not disabled");
    good(delete_column.activate()); require(take(source->snapshot())->revision == 6, "The last column was deleted");
    good(menu.close_popup());
    input.structure_controls_visible = false; good(table->set_options(input));
    require(!take(rows.snapshot()).visible && !take(columns.snapshot()).visible && take(table->frame())->width == 220,
            "Hiding controls did not restore the viewport");
    body = take(table->body().snapshot());
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body.bounds.x+40, .y=body.bounds.y+12, .target=body.id}));
    retire_on_clear = true; good(delete_row.activate());
    require(take(source->snapshot())->revision == 7 && !table->root().snapshot() && changes == 3,
            "Retiring the table during a selection callback dispatched into a dead view");
    tree->close();

    // The controls use visible, scrolled geometry but commit using stable model keys.
    std::vector<lvui::TableColumn> many_columns; std::vector<lvui::TableRow> many_rows;
    for (int i=0; i<8; ++i) many_columns.push_back({.key="c"+std::to_string(i), .width=80});
    for (int i=0; i<30; ++i) many_rows.push_back({.key="row"+std::to_string(i), .cells=std::vector<lvui::TableCell>(8)});
    auto scrolled_source = std::make_shared<lvui::TableMemory>(std::move(many_columns), std::move(many_rows));
    lvui::native::TableInput scrolled_input; scrolled_input.controller = std::make_shared<lvui::TableController>(scrolled_source);
    scrolled_input.structure_editor = std::make_shared<lvui::TableMemoryStructureEditor>(scrolled_source);
    scrolled_input.structure_controls_visible = true; scrolled_input.appearance.width = 220; scrolled_input.appearance.height = 160;
    auto scrolled_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto scrolled_host = take(scrolled_tree->content_host({}, "host", {500, 300})); std::optional<lvui::native::NativeTable> scrolled_table;
    good(scrolled_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "stack", {.alignment=StackAlignment::leading}); if (!stack) return std::unexpected(stack.error());
        auto value = lvui::native::NativeTable::create(builder, *stack, "scrolled", scrolled_input);
        if (!value) return std::unexpected(value.error()); scrolled_table = *value; return {};
    }));
    good(scrolled_table->scroll().set_scroll_position(170, 91));
    auto frame = take(scrolled_table->frame());
    const auto visible_row = std::ranges::find_if(frame->rows, [&](const auto& row) { return row.offset >= 0 && row.offset+row.extent <= frame->height; });
    require(visible_row != frame->rows.end() && visible_row->index > 0, "Vertical scrolling did not reveal later rows");
    const auto row_index = visible_row->index; const auto row_y = visible_row->offset + visible_row->extent/2;
    const auto scrolled_nodes = take(scrolled_tree->snapshots());
    const auto gutter_node = std::ranges::find_if(scrolled_nodes, [](const auto& item) { return item.id.ends_with(".row-controls"); });
    require(gutter_node != scrolled_nodes.end(), "Scrolled row controls were not mounted");
    auto position = gutter_node->bounds;
    good(scrolled_tree->input({.kind=WidgetInputKind::pointer_down, .x=position.x+10, .y=position.y+row_y, .buttons=1}));
    good(scrolled_tree->input({.kind=WidgetInputKind::pointer_up, .x=position.x+10, .y=position.y+row_y, .buttons=1}));
    require(take(scrolled_source->snapshot())->rows[row_index+1].key == "lvui-row-1", "Scrolled row insertion targeted the wrong row");
    frame = take(scrolled_table->frame());
    const auto visible_column = std::ranges::find_if(frame->columns, [&](const auto& column) { return column.offset >= 0 && column.offset+column.extent <= frame->width; });
    require(visible_column != frame->columns.end() && visible_column->index > 0, "Horizontal scrolling did not reveal later columns");
    const auto column_index = visible_column->index; const auto column_x = visible_column->offset + visible_column->extent/2;
    const auto refreshed_nodes = take(scrolled_tree->snapshots());
    const auto column_node = std::ranges::find_if(refreshed_nodes, [](const auto& item) { return item.id.ends_with(".column-controls"); });
    require(column_node != refreshed_nodes.end(), "Scrolled column controls were not mounted"); position = column_node->bounds;
    good(scrolled_tree->input({.kind=WidgetInputKind::pointer_down, .x=position.x+column_x, .y=position.y+10, .buttons=1}));
    good(scrolled_tree->input({.kind=WidgetInputKind::pointer_up, .x=position.x+column_x, .y=position.y+10, .buttons=1}));
    require(take(scrolled_source->snapshot())->columns[column_index+1].key == "lvui-column-1", "Scrolled column insertion targeted the wrong column");
    (void)take(scrolled_input.controller->set_mode(lvui::TableSelectionMode::none));
    auto unselected_body = take(scrolled_table->body().snapshot());
    good(scrolled_tree->input({.kind=WidgetInputKind::context_request, .x=unselected_body.bounds.x+30,
        .y=unselected_body.bounds.y+12, .target=unselected_body.id}));
    const auto context_nodes = take(scrolled_tree->snapshots());
    const auto unselected_menu = std::ranges::find_if(context_nodes, [](const auto& item) { return item.id.ends_with(".structure-menu"); });
    require(unselected_menu != context_nodes.end() && unselected_menu->opened && !take(scrolled_input.controller->selection()),
            "Disabled selection prevented opening a structure menu or selected a cell");
    good(take(scrolled_tree->find(unselected_menu->id)).close_popup());
    auto reentrant = std::make_shared<ReentrantFactory>(); scrolled_input.structure_factory = reentrant; scrolled_input.source_revision = 2;
    good(scrolled_table->set_options(scrolled_input));
    reentrant->change = [&] { scrolled_input.appearance.width = 230; good(scrolled_table->set_options(scrolled_input)); };
    frame = take(scrolled_table->frame());
    const auto reentrant_row = std::ranges::find_if(frame->rows, [&](const auto& row) { return row.offset >= 0 && row.offset+row.extent <= frame->height; });
    require(reentrant_row != frame->rows.end(), "Reentrant setup did not have a visible row");
    const auto final_nodes = take(scrolled_tree->snapshots());
    const auto final_gutter = std::ranges::find_if(final_nodes, [](const auto& item) { return item.id.ends_with(".row-controls"); });
    require(final_gutter != final_nodes.end(), "Reentrant setup lost its controls"); position = final_gutter->bounds;
    const auto action_y = position.y + reentrant_row->offset + reentrant_row->extent/2;
    good(scrolled_tree->input({.kind=WidgetInputKind::pointer_down, .x=position.x+10, .y=action_y, .buttons=1}));
    auto stale = scrolled_tree->input({.kind=WidgetInputKind::pointer_up, .x=position.x+10, .y=action_y, .buttons=1});
    require(!stale && take(scrolled_source->snapshot())->revision == 2,
            "A factory that replaced table options during preparation committed a stale action");
    good(scrolled_host.clear_content()); scrolled_tree->close();
}
