#include <LVUI/TableStructure.h>

#include <atomic>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    auto source = std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="a", .width=60}, {.key="b", .width=60}},
        std::vector<TableRow>{{.key="r0", .cells={{.text="A"}, {.text="B"}}}, {.key="r1", .cells={{.text="C"}, {.text="D"}}}});
    auto original = take(source->snapshot()); auto controller = std::make_shared<TableController>(source);
    require(take(controller->select({"r0", "a"})), "Table selection setup failed");
    TableStructureEdit edit{.revision=0, .operations={
        TableInsertRows{.index=1, .rows={{.key="r2", .cells={{.text="X"}, {.text="Y"}}}}},
        TableInsertColumns{.index=1, .columns={{.key="c", .width=80}}, .cells={{{.text="A2"}}, {{.text="X2"}}, {{.text="C2"}}}},
        TableResizeRows{.rows={{.key="r0", .height=40}}},
        TableResizeColumns{.columns={{.key="a", .width=100}}},
        TableRemoveRows{.keys={"r1"}}, TableRemoveColumns{.keys={"b"}}}};
    auto candidate = take(prepare_table_structure(original, edit));
    require(candidate.revision == 0 && candidate.columns.size() == 2 && candidate.rows.size() == 2 &&
            candidate.columns[0].key == "a" && candidate.columns[0].width == 100 && candidate.columns[1].key == "c" &&
            candidate.rows[0].height == 40 && candidate.rows[0].cells[1].text == "A2" && candidate.rows[1].cells[1].text == "X2" &&
            take(source->snapshot())->revision == 0, "Preparing a structure batch changed its source or misaligned cells");
    TableMemoryStructureEditor editor(source); require(take(editor.apply(edit)) == 1, "Structure batch did not commit once");
    auto changed = take(source->snapshot());
    require(changed->rows.size() == 2 && changed->rows[1].key == "r2" && changed->rows[1].cells[0].text == "X" &&
            original->rows.size() == 2 && original->columns[1].key == "b" && take(controller->selection())->current.row == "r0",
            "Structure commit lost data, immutable history or key-based selection");
    require(!editor.apply(edit) && take(source->snapshot())->revision == 1, "A stale structure edit overwrote newer data");
    TableStructureEdit rejected{.revision=1, .operations={
        TableInsertRows{.index=2, .rows={{.key="r3", .cells={{}, {}}}}}, TableResizeColumns{.columns={{.key="missing", .width=50}}}}};
    require(!editor.apply(rejected) && take(source->snapshot())->revision == 1 && take(source->snapshot())->rows.size() == 2,
            "A failed structure batch partially inserted a row");
    require(!editor.apply({.revision=1, .operations={TableInsertRows{.index=0, .rows={{.key="r0", .cells={{}, {}}}}}}}) &&
            !editor.apply({.revision=1, .operations={TableRemoveRows{.keys={"r0", "r0"}}}}) &&
            !editor.apply({.revision=1, .operations={TableInsertColumns{.index=1, .columns={{.key="d"}}, .cells={{{}}}}}}) &&
            !editor.apply({.revision=1, .operations={TableResizeColumns{.columns={{.key="a", .width=12}}}}}) &&
            !editor.apply({.revision=1, .operations={TableResizeRows{.rows={{.key="r0", .height=std::numeric_limits<double>::infinity()}}}}}),
            "Invalid structure keys, dimensions or cell data were accepted");
    require(take(editor.apply({.revision=1, .operations={TableResizeRows{.rows={{.key="r0", .height=40}}}}})) == 1,
            "A no-op structure edit published a revision");
    TableStructureEdit left{.revision=1, .operations={TableResizeRows{.rows={{.key="r0", .height=44}}}}};
    TableStructureEdit right{.revision=1, .operations={TableResizeColumns{.columns={{.key="a", .width=110}}}}};
    std::atomic<int> winners{0};
    std::thread a([&] { if (editor.apply(left)) ++winners; }); std::thread b([&] { if (editor.apply(right)) ++winners; });
    a.join(); b.join(); require(winners == 1 && take(source->snapshot())->revision == 2,
        "Concurrent table structure edits both committed the same revision");
}
