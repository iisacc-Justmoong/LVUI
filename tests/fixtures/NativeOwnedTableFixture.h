#pragma once
#include <LVUI/TableSorting.h>
#include <LVUI/TableStructure.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct NativeOwnedTableModel final {
    std::shared_ptr<lvui::TableMemory> source = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="a", .title="Alpha", .width=80}, {.key="b", .title="Beta", .width=80}},
        std::vector<lvui::TableRow>{{.key="r0", .cells={{.text="First"}, {.text="Second"}}},
                                    {.key="r1", .cells={{.text="Third"}, {.text="Fourth"}}},
                                    {.key="r2", .cells={{.text="Fifth"}, {.text="Sixth"}}}});
    ii::core::Observable<std::shared_ptr<lvui::TableController>> controller{std::make_shared<lvui::TableController>(source)};
    ii::core::Observable<std::shared_ptr<lvui::TableEditor>> editor{source};
    ii::core::Observable<std::shared_ptr<lvui::TableOrderEditor>> order{std::make_shared<lvui::TableMemoryOrderEditor>(source)};
    ii::core::Observable<std::shared_ptr<lvui::TableStructureEditor>> structure{std::make_shared<lvui::TableMemoryStructureEditor>(source)};
    ii::core::Observable<bool> editable{true};
    ii::core::Observable<bool> clipboard{true};
    ii::core::Observable<bool> sorting{true};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<double> width{200};
    ii::core::Observable<bool> selection{true};
    int events = 0, edits = 0, submissions = 0, rejections = 0, ranges = 0, sorts = 0, structures = 0, columns = 0, rows = 0;
    std::optional<lvui::TableSelection> last; std::optional<lvui::TableRange> last_range;
    std::optional<lvui::TableSortOrder> last_sort;
    void edited(std::size_t, std::size_t, std::string) { ++edits; }
    void submitted(std::size_t, std::size_t, std::string) { ++submissions; revision.set(std::int64_t(source->snapshot().value()->revision)); }
    void rejected(std::size_t, std::size_t, std::string, std::string) { ++rejections; }
    void rangeChanged(const lvui::TableRange& value) { ++ranges; last_range = value; revision.set(std::int64_t(source->snapshot().value()->revision)); }
    void sorted(std::size_t, lvui::TableSortOrder value) { ++sorts; last_sort = value; revision.set(std::int64_t(source->snapshot().value()->revision)); }
    void structured(const lvui::TableStructureEdit&) { ++structures; revision.set(std::int64_t(source->snapshot().value()->revision)); }
    void columnResized(std::size_t, double) { ++columns; }
    void rowResized(std::size_t, double) { ++rows; }
    void selected(std::optional<lvui::TableSelection> value) { ++events; last = std::move(value); }
};
