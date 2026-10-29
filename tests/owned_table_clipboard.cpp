#include <LVUI/TableClipboard.h>

#include <charconv>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class IntegerEditor final : public lvui::TableEditor {
public:
    explicit IntegerEditor(std::shared_ptr<lvui::TableMemory> value) : source(std::move(value)) {}
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& edit) override {
        for (const auto& cell : edit.cells) {
            int value = 0; const auto parsed = std::from_chars(cell.text.data(), cell.text.data() + cell.text.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != cell.text.data() + cell.text.size())
                return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Expected an integer"});
        }
        return source->edit(edit);
    }
    std::shared_ptr<lvui::TableMemory> source;
};
}

int main() {
    using namespace lvui;
    auto source = std::make_shared<TableMemory>(
        std::vector<TableColumn>{{.key="a"}, {.key="b"}, {.key="c"}},
        std::vector<TableRow>{{.key="one", .cells={{.text="한글"}, {.text="line\nnext"}, {.text="a\tb"}}},
                              {.key="two", .cells={{.text="quote \"x\""}, {.text=""}, {.text="plain"}}},
                              {.key="three", .cells={{.text="old"}, {.text="old"}, {.text="old", .editable=false}}}});
    auto original = take(source->snapshot());
    TableRange rectangle{0, 1, 0, 2};
    const auto encoded = take(serialize_table_clipboard(original, rectangle));
    require(encoded == "한글\t\"line\nnext\"\t\"a\tb\"\n\"quote \"\"x\"\"\"\t\tplain", "Table clipboard serialization lost quoting or UTF-8");
    const auto parsed = take(parse_table_clipboard(encoded));
    require(parsed.rows.size() == 2 && parsed.rows[0][1] == "line\nnext" && parsed.rows[0][2] == "a\tb" &&
            parsed.rows[1][0] == "quote \"x\"" && parsed.rows[1][1].empty(), "Table clipboard did not round-trip quoted values");
    auto last_empty = take(serialize_table_clipboard(original, {0, 1, 1, 1}));
    require(take(parse_table_clipboard(last_empty)).rows == std::vector<std::vector<std::string>>{{"line\nnext"}, {""}},
            "A trailing empty row disappeared from a one-column clipboard selection");
    require(take(parse_table_clipboard("x\t\r\ny\tz\r\n")).rows == std::vector<std::vector<std::string>>{{"x", ""}, {"y", "z"}} &&
            take(parse_table_clipboard("")).rows == std::vector<std::vector<std::string>>{{""}} &&
            take(parse_table_clipboard("a\n\n")).rows == std::vector<std::vector<std::string>>{{"a"}, {""}},
            "Table clipboard mishandled record terminators or empty fields");
    require(!parse_table_clipboard("\"unclosed") && !parse_table_clipboard("\"closed\"extra") &&
            !parse_table_clipboard("a\"b") && !parse_table_clipboard("a\tb\nc") &&
            !parse_table_clipboard(std::string("bad\xff", 4)) && !parse_table_clipboard(std::string(65537, 'a')) &&
            !parse_table_clipboard(std::string(table_clipboard_maximum_bytes + 1, 'a')),
            "Malformed, uneven, invalid UTF-8 or oversized table clipboard data was accepted");
    auto expansion = take(prepare_table_paste(original, {1, 1, 0, 0}, take(parse_table_clipboard("A\tB\nC\tD"))));
    require(expansion.revision == 0 && expansion.cells.size() == 4 && expansion.cells[2].row == "three" &&
            take(source->snapshot())->revision == 0, "Preparing a table paste changed source data or lost its target");
    require(take(source->edit(expansion)) == 1 && take(source->snapshot())->rows[2].cells[1].text == "D" &&
            original->rows[1].cells[0].text == "quote \"x\"", "Table paste was not submitted as one atomic edit");
    require(!source->edit(expansion), "A stale prepared table paste overwrote a newer document");
    auto current = take(source->snapshot());
    auto fill = take(prepare_table_paste(current, {0, 1, 0, 1}, take(parse_table_clipboard("fill"))));
    require(fill.cells.size() == 4 && take(source->edit(fill)) == 2 && take(source->snapshot())->rows[1].cells[1].text == "fill",
            "A single clipboard cell did not fill the selected range");
    current = take(source->snapshot());
    require(!prepare_table_paste(current, {2, 2, 2, 2}, take(parse_table_clipboard("locked"))) &&
            !prepare_table_paste(current, {1, 1, 1, 1}, take(parse_table_clipboard("1\t2\n3\t4\n5\t6"))) &&
            !prepare_table_paste(current, {0, 1, 0, 2}, take(parse_table_clipboard("1\t2\n3\t4"))) &&
            !prepare_table_paste(current, {0, 0, 0, 0}, TableClipboardData{{{"a", "b"}, {"c"}}}) &&
            !serialize_table_clipboard(current, {0, 2, 0, 3}) && take(source->snapshot())->revision == 2,
            "A read-only, out-of-bounds or mismatched paste was accepted or partially changed data");
    IntegerEditor numbers(source);
    auto typed = take(prepare_table_paste(current, {0, 1, 0, 1}, take(parse_table_clipboard("1\t2\n3\tinvalid"))));
    require(!numbers.edit(typed) && take(source->snapshot())->revision == 2 && take(source->snapshot())->rows[0].cells[0].text == "fill",
            "A custom validator partially committed a rejected clipboard range");
    std::string wide_row;
    for (std::size_t i = 0; i < table_clipboard_maximum_cells + 1; ++i) { if (i) wide_row += '\t'; wide_row += 'x'; }
    require(!parse_table_clipboard(wide_row), "Clipboard accepted an unbounded number of cells");
    require(bool(prepare_table_paste(current, {0, 1, 0, 1}, TableClipboardData{{{std::string(65536, 'x')}}})),
            "A bounded paste was unexpectedly rejected");
    std::vector<TableColumn> wide_columns; std::vector<TableCell> wide_cells;
    for (int i = 0; i < 17; ++i) { wide_columns.push_back({.key=std::to_string(i)}); wide_cells.push_back({}); }
    auto wide = std::make_shared<TableMemory>(std::move(wide_columns), std::vector<TableRow>{{.key="wide", .cells=std::move(wide_cells)}});
    require(!prepare_table_paste(take(wide->snapshot()), {0, 0, 0, 16}, TableClipboardData{{{std::string(65536, 'x')}}}),
            "Expanding a large clipboard cell exceeded the paste allocation budget");
}
