#include <LVUI/Table.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class ReadOnly final : public lvui::TableSource {
public:
    explicit ReadOnly(std::shared_ptr<const lvui::TableSnapshot> source) : value(std::move(source)) {}
    lvui::TableResult<std::shared_ptr<const lvui::TableSnapshot>> snapshot() const override { return value; }
    std::shared_ptr<const lvui::TableSnapshot> value;
};
}
int main() {
    using namespace lvui;
    std::vector<TableColumn> columns{{.key="name", .title="Name", .width=120},
                                     {.key="kind", .title="Kind"}, {.key="value", .title="Value", .width=80}};
    std::vector<TableRow> rows{{.key="first", .cells={{.text="A"}, {.text="Photo"}, {.text="10"}}},
                               {.key="second", .cells={{.text="B"}, {.text="Audio"}, {.text="20"}}}};
    auto memory = std::make_shared<TableMemory>(columns, rows); auto original = take(memory->snapshot());
    auto index = take(TableIndex::create(original));
    require(index.row("second") == 1 && index.column("value") == 2 && !index.row("missing"), "Table key index is incorrect");
    auto readonly = std::make_shared<ReadOnly>(original); TableController view(readonly);
    require(take(view.select({"first", "value"})) && take(view.select({"second", "name"}, true)), "Range selection failed");
    auto selected = take(view.range());
    require(selected && selected->first_row == 0 && selected->last_row == 1 && selected->first_column == 0 && selected->last_column == 2 &&
            take(memory->snapshot())->revision == 0, "Table selection changed document state or range bounds");
    require(take(view.set_mode(TableSelectionMode::single)) && take(view.range())->first_row == 1 &&
            take(view.range())->first_column == 0, "Switching to single selection did not preserve the current cell");
    require(take(view.set_mode(TableSelectionMode::none)) && !take(view.selection()) && !view.select({"first", "name"}),
            "Disabled table selection retained or accepted a selection");
    require(take(view.set_mode(TableSelectionMode::range)) && take(view.select({"first", "value"})) &&
            take(view.select({"second", "name"}, true)), "Table selection did not recover");

    TableEdit edit{.revision=0, .cells={{"first", "name", "Edited"}, {"second", "value", "42"}}};
    require(take(memory->edit(edit)) == 1, "Atomic table edit failed");
    auto edited = take(memory->snapshot());
    require(edited->rows[0].cells[0].text == "Edited" && edited->rows[1].cells[2].text == "42" &&
            original->rows[0].cells[0].text == "A", "Table edit lost a value or mutated an earlier snapshot");
    require(!memory->edit(edit) && take(memory->snapshot())->revision == 1, "A stale table edit changed the source");
    edit.revision = 1; require(take(memory->edit(edit)) == 1, "An unchanged table edit published a revision");
    TableEdit duplicate{.revision=1, .cells={{"first", "name", "X"}, {"first", "name", "Y"}}};
    require(!memory->edit(duplicate) && take(memory->snapshot())->rows[0].cells[0].text == "Edited",
            "A duplicate table edit was partially applied");
    auto readonly_rows = edited->rows; readonly_rows[1].cells[2].editable = false;
    require(take(memory->replace(edited->columns, readonly_rows, 1)) == 2, "Read-only candidate failed");
    TableEdit denied{.revision=2, .cells={{"first", "name", "X"}, {"second", "value", "Y"}}};
    require(!memory->edit(denied) && take(memory->snapshot())->rows[0].cells[0].text == "Edited",
            "A rejected table batch partially changed another cell");
    auto invalid_text = denied; invalid_text.cells = {{"first", "name", std::string("bad\xff", 4)}};
    require(!memory->edit(invalid_text), "An invalid UTF-8 table edit was accepted");
    auto missing = denied; missing.cells = {{"absent", "name", "X"}};
    require(!memory->edit(missing), "A nonexistent table cell was edited");

    auto changed = take(memory->snapshot()); readonly->value = changed;
    require(take(view.selection()) && take(view.source_snapshot()) == changed, "A surviving selection was not retained after a source update");
    auto reordered = changed->rows; std::swap(reordered[0], reordered[1]);
    require(take(memory->replace(changed->columns, reordered, 2)) == 3, "Table row replacement failed");
    readonly->value = take(memory->snapshot()); selected = take(view.range());
    require(selected && selected->first_row == 0 && selected->last_row == 1 && take(view.selection())->current.row == "second",
            "A reordered table lost key-based view selection");
    auto shortened_columns = columns; shortened_columns.pop_back(); auto shortened_rows = reordered;
    for (auto& row : shortened_rows) row.cells.pop_back();
    require(take(memory->replace(shortened_columns, shortened_rows, 3)) == 4, "Removing a table column failed");
    readonly->value = take(memory->snapshot()); require(!take(view.selection()), "Removed selection key was not pruned");
    require(take(view.select({"first", "name"})) && !take(view.select({"first", "name"})), "A repeated selection changed view state");
    require(take(view.navigate(TableDirection::right)) && take(view.selection())->current.column == "kind" &&
            take(view.navigate(TableDirection::up, true)) && take(view.range())->first_row == 0 && take(view.range())->last_row == 1 &&
            take(view.selection())->current.row == "second", "Table navigation or extending its selection failed");
    require(take(view.navigate(TableDirection::home)) && take(view.selection())->current.column == "name" &&
            take(view.navigate(TableDirection::end)) && take(view.selection())->current.column == "kind" &&
            !take(view.navigate(TableDirection::right)) && take(view.clear_selection()) && !take(view.clear_selection()) &&
            take(view.navigate(TableDirection::down)) && take(view.selection())->current.row == "second" &&
            take(memory->snapshot())->revision == 4, "Table navigation limits, clear or document isolation failed");
    auto blocked_rows = rows; blocked_rows[0].cells[1].enabled = false; blocked_rows[1].cells[2].enabled = false;
    auto blocked_source = std::make_shared<TableMemory>(columns, blocked_rows); TableController blocked(blocked_source);
    require(take(blocked.select({"first", "name"})) && take(blocked.navigate(TableDirection::right)) &&
            take(blocked.selection())->current.column == "value" && !take(blocked.navigate(TableDirection::down)) &&
            take(blocked_source->snapshot())->revision == 0, "Table navigation did not skip disabled cells");
    bool wrong_thread = false;
    std::thread wrong([&] { auto result = view.selection(); wrong_thread = !result && result.error().code == TableErrorCode::wrong_thread; });
    wrong.join(); require(wrong_thread, "Table controller accepted a foreign thread");
    auto broken = shortened_rows; broken[1].key = broken[0].key;
    require(!memory->replace(shortened_columns, broken, 4), "Duplicate table row keys were accepted");
    auto too_many_columns = std::vector<TableColumn>(table_maximum_columns + 1);
    require(!TableIndex::create(std::make_shared<TableSnapshot>(TableSnapshot{std::move(too_many_columns), {}, 0})),
            "Table accepted too many columns");
    std::atomic<int> winners{0};
    std::thread left([&] { if (memory->edit({.revision=4, .cells={{"first", "name", "Left"}}})) ++winners; });
    std::thread right([&] { if (memory->edit({.revision=4, .cells={{"first", "name", "Right"}}})) ++winners; });
    left.join(); right.join();
    require(winners == 1 && take(memory->snapshot())->revision == 5, "Concurrent table edits both committed the same revision");
}
