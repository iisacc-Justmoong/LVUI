#include <LVUI/TableViewport.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    std::vector<TableColumn> columns{{.key="a", .width=120}, {.key="b", .width=140}, {.key="c", .width=100}};
    std::vector<TableRow> rows{{.key="r0", .cells={{}, {}, {}}, .height=24},
                               {.key="r1", .cells={{}, {}, {}}, .height=40},
                               {.key="r2", .cells={{}, {}, {}}, .height=24}};
    auto memory = std::make_shared<TableMemory>(columns, rows); auto snapshot = take(memory->snapshot());
    auto source = take(TableViewportSource::create(snapshot, {.width=160}));
    TableViewportRequest request{.height=64, .header_height=24, .x=125, .y=29, .overscan_rows=0, .overscan_columns=0};
    auto frame = take(source->frame(request));
    require(frame.content_width == 360 && frame.content_height == 88 && frame.x == 125 && frame.y == 29 &&
            frame.rows.size() == 2 && frame.rows.front().index == 1 && frame.rows.front().offset == 19 &&
            frame.columns.size() == 2 && frame.columns.front().index == 1 && frame.columns.front().offset == -5 &&
            frame.anchor.row == "r1" && frame.anchor.column == "b" && frame.anchor.within_y == 5 && frame.anchor.within_x == 5,
            "Table viewport geometry or anchor is incorrect");
    columns.insert(columns.begin(), {.key="inserted-column", .width=50});
    for (auto& row : rows) row.cells.insert(row.cells.begin(), TableCell{});
    rows.insert(rows.begin(), {.key="inserted-row", .cells={{}, {}, {}, {}}, .height=24});
    require(take(memory->replace(columns, rows, 0)) == 1, "Inserted table geometry was rejected");
    auto changed = take(TableViewportSource::create(take(memory->snapshot()), {.width=160}));
    request.anchor = frame.anchor;
    auto anchored = take(changed->frame(request));
    require(anchored.x == 175 && anchored.y == 53 && anchored.anchor.row == "r1" && anchored.anchor.column == "b",
            "Table insertion did not preserve the two-axis visible anchor");
    request.horizontal_movement = 1;
    auto moved = take(changed->frame(request));
    require(moved.x == 125 && moved.y == 53, "Horizontal user movement did not override only its own old anchor");
    request.anchor = {}; request.x = request.y = 0; request.reveal_row = "r2"; request.reveal_column = "c";
    request.reveal = TableReveal::end;
    auto revealed = take(changed->frame(request));
    require(revealed.x == 250 && revealed.y == 72 && revealed.rows.back().index == 3 && revealed.columns.back().index == 3,
            "Revealing the final table cell did not clamp both axes");
    request.reveal_row = "absent"; require(!changed->frame(request), "A nonexistent reveal row was accepted");
    request.reveal_row.clear(); request.reveal_column.clear(); request.overscan_rows = 129;
    require(!changed->frame(request), "Unbounded table overscan was accepted");
    require(snapshot->rows.size() == 3 && snapshot->columns.size() == 3, "Table viewport changed an earlier snapshot");

    std::vector<TableColumn> flexible{{.key="fixed", .width=100}, {.key="flex1"}, {.key="flex2"}};
    auto fit = take(TableViewportSource::create(std::make_shared<TableSnapshot>(TableSnapshot{flexible, {}, 0}), {.width=300}));
    auto fit_frame = take(fit->frame({.height=40, .overscan_columns=0}));
    require(fit_frame.content_width == 300 && fit_frame.columns.size() == 3 && fit_frame.columns[1].extent == 100 &&
            fit_frame.columns[2].extent == 100, "Flexible table columns did not share the remaining viewport width");
    auto natural = take(TableViewportSource::create(std::make_shared<TableSnapshot>(TableSnapshot{flexible, {}, 0}),
        {.width=200, .fallback_column_width=120, .fit_flexible_columns=false}));
    require(take(natural->frame({})).content_width == 340, "Natural table columns did not use the fallback width");
    require(!TableViewportSource::create(snapshot, {.width=0}) &&
            !TableViewportSource::create(snapshot, {.width=200, .fallback_column_width=20}), "Invalid table viewport configuration was accepted");
    std::vector<TableColumn> wide; for (int i = 0; i < 20; ++i) wide.push_back({.key="c" + std::to_string(i), .width=40});
    std::vector<TableRow> large; large.reserve(10000);
    for (int i = 0; i < 10000; ++i) { TableRow row; row.key = "r" + std::to_string(i); row.cells.resize(wide.size()); large.push_back(std::move(row)); }
    auto large_snapshot = std::make_shared<TableSnapshot>(TableSnapshot{std::move(wide), std::move(large), 7});
    auto virtualized = take(TableViewportSource::create(large_snapshot, {.width=240}));
    auto deep = take(virtualized->frame({.height=120, .x=300, .y=100000}));
    require(deep.source_revision == 7 && deep.rows.size() <= 7 && deep.columns.size() <= 9 &&
            deep.rows.front().index > 4000 && deep.columns.front().index > 0,
            "Large table scrolling materialized the entire grid or lost its logical position");
}
