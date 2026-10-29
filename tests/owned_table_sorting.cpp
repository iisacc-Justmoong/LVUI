#include <LVUI/TableSorting.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class NumericSorter final : public lvui::TableSorter {
public:
    lvui::TableResult<std::vector<std::string>> order(const lvui::TableSnapshot& data, std::size_t column,
                                                     lvui::TableSortOrder direction) const override {
        std::vector<std::pair<int, std::string>> values;
        for (const auto& row : data.rows) {
            int number = 0; const auto& text = row.cells[column].text;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Expected a number"});
            values.emplace_back(number, row.key);
        }
        std::stable_sort(values.begin(), values.end(), [&](const auto& a, const auto& b) {
            return direction == lvui::TableSortOrder::ascending ? a.first < b.first : b.first < a.first;
        });
        std::vector<std::string> rows; for (auto& value : values) rows.push_back(std::move(value.second)); return rows;
    }
};
class InvalidSorter final : public lvui::TableSorter {
public:
    lvui::TableResult<std::vector<std::string>> order(const lvui::TableSnapshot&, std::size_t, lvui::TableSortOrder) const override {
        return std::vector<std::string>{"b", "b", "a", "z"};
    }
};
}

int main() {
    using namespace lvui;
    auto source = std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="value"}, {.key="locked", .enabled=false}},
        std::vector<TableRow>{{.key="b", .cells={{.text="2"}, {}}}, {.key="a", .cells={{.text="10"}, {}}},
                              {.key="c", .cells={{.text="2"}, {}}}, {.key="z", .cells={{.text="1"}, {}}}});
    auto original = take(source->snapshot()); TableTextSorter text; NumericSorter numeric; InvalidSorter invalid;
    auto ascending = take(prepare_table_sort(original, "value", TableSortOrder::ascending, text));
    require(ascending.rows == std::vector<std::string>{"z", "a", "b", "c"} && ascending.revision == 0 &&
            take(source->snapshot())->revision == 0, "Stable text sort planning changed data or misordered equal values");
    auto descending = take(prepare_table_sort(original, "value", TableSortOrder::descending, text));
    auto numeric_order = take(prepare_table_sort(original, "value", TableSortOrder::ascending, numeric));
    require(descending.rows == std::vector<std::string>{"b", "c", "a", "z"} &&
            numeric_order.rows == std::vector<std::string>{"z", "b", "c", "a"}, "Descending or custom numeric sort was incorrect");
    require(!prepare_table_sort(original, "missing", TableSortOrder::ascending, text) &&
            !prepare_table_sort(original, "locked", TableSortOrder::ascending, text) &&
            !prepare_table_sort(original, "value", static_cast<TableSortOrder>(8), text) &&
            !prepare_table_sort(original, "value", TableSortOrder::ascending, invalid), "Invalid sort policy or column was accepted");
    auto malformed = *original; malformed.rows[0].cells.clear();
    require(!text.order(malformed, 0, TableSortOrder::ascending), "Text sorter accepted a row without the sorted cell");
    auto controller = std::make_shared<TableController>(source); require(take(controller->select({"b", "value"})), "Selection setup failed");
    auto editor = std::make_shared<TableMemoryOrderEditor>(source);
    require(take(editor->reorder(ascending)) == 1 && take(source->snapshot())->rows.front().key == "z" &&
            take(controller->selection())->current.row == "b" && original->rows.front().key == "b",
            "Sort did not preserve keyed selection or the old immutable snapshot");
    require(!editor->reorder(descending) && take(source->snapshot())->revision == 1, "A stale sort overwrote the document");
    auto current = take(source->snapshot()); auto no_op = take(prepare_table_sort(current, "value", TableSortOrder::ascending, text));
    require(take(editor->reorder(no_op)) == 1, "A no-op sort published a document revision");
    auto bad_plan = no_op; bad_plan.rows[0] = bad_plan.rows[1];
    require(!editor->reorder(bad_plan) && take(source->snapshot())->revision == 1, "A duplicate sort plan changed the document");
    auto left = take(prepare_table_sort(current, "value", TableSortOrder::descending, text));
    auto right = take(prepare_table_sort(current, "value", TableSortOrder::ascending, numeric));
    std::atomic<int> winners{0};
    std::thread a([&] { if (editor->reorder(left)) ++winners; });
    std::thread b([&] { if (editor->reorder(right)) ++winners; });
    a.join(); b.join();
    require(winners == 1 && take(source->snapshot())->revision == 2, "Concurrent table sorts both published the same revision");
}
