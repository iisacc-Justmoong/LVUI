#include <LVUI/NativeTable.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
bool has_text(const ii::gui::CanvasFrame& drawing, std::string_view value) {
    for (const auto& text : drawing.texts) if (text.text == value) return true;
    for (const auto& layer : drawing.overlays) for (const auto& text : layer.texts) if (text.text == value) return true;
    return false;
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
        return std::vector<std::string>{"r0", "r0", "r1", "r3"};
    }
};
class ChangingSorter final : public lvui::TableSorter {
public:
    std::shared_ptr<lvui::TableMemory> source;
    lvui::TableResult<std::vector<std::string>> order(const lvui::TableSnapshot& data, std::size_t column,
                                                     lvui::TableSortOrder direction) const override {
        auto changed = source->edit({.revision=data.revision, .cells={{"r0", "value", "9"}}});
        if (!changed) return std::unexpected(changed.error());
        return lvui::TableTextSorter{}.order(data, column, direction);
    }
};
class PublishingEditor final : public lvui::TableOrderEditor {
public:
    std::shared_ptr<lvui::TableOrderEditor> editor; std::function<void()> changed;
    lvui::TableResult<std::uint64_t> reorder(const lvui::TableSortPlan& plan) override {
        auto result = editor->reorder(plan); if (result && changed) changed(); return result;
    }
};
}

int main() {
    using namespace ii::gui; using lvui::TableSortOrder;
    auto source = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="value", .title="Value", .width=100}, {.key="disabled", .title="Locked", .width=100, .enabled=false}},
        std::vector<lvui::TableRow>{{.key="r0", .cells={{.text="2"}, {}}}, {.key="r1", .cells={{.text="10"}, {}}},
                                   {.key="r2", .cells={{.text="2"}, {}}}, {.key="r3", .cells={{.text="1"}, {}}}});
    auto order = std::make_shared<lvui::TableMemoryOrderEditor>(source);
    lvui::native::TableInput input; input.controller = std::make_shared<lvui::TableController>(source);
    input.order_editor = order; input.sorting_enabled = true; input.appearance.width = 220; input.appearance.height = 130;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 150})); std::optional<lvui::native::NativeTable> table;
    int notifications = 0; std::optional<TableSortOrder> direction; std::size_t column = 99;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        auto widget = lvui::native::NativeTable::create(builder, *stack, "table", input,
            {.sorted = [&](std::size_t value, TableSortOrder order_value) { ++notifications; column = value; direction = order_value; }});
        if (!widget) return std::unexpected(widget.error()); table = *widget; return {};
    }));
    (void)take(table->frame());
    const auto send = [&](WidgetInputKind kind, double x, double y) { good(tree->input({.kind=kind, .x=x, .y=y, .buttons=1})); };
    const auto click = [&](double x, double y) { send(WidgetInputKind::pointer_down, x, y); send(WidgetInputKind::pointer_up, x, y); };
    click(30, 34); click(30, 10);
    auto current = take(source->snapshot());
    require(current->revision == 1 && current->rows[0].key == "r3" && current->rows[1].key == "r1" &&
            take(input.controller->selection())->current.row == "r0" && notifications == 1 && column == 0 && direction == TableSortOrder::ascending &&
            has_text(take(tree->drawing_snapshot({240, 150})), "↑"),
            "Header sort lost selection, stable ordering or its callback");
    click(30, 10); current = take(source->snapshot());
    require(current->revision == 2 && current->rows[0].key == "r0" && current->rows[1].key == "r2" &&
            notifications == 2 && direction == TableSortOrder::descending && has_text(take(tree->drawing_snapshot({240, 150})), "↓"),
            "Header did not reverse a stable table sort");
    click(150, 10); require(take(source->snapshot())->revision == 2 && notifications == 2, "A disabled table column sorted");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::right}));
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::enter}));
    require(take(source->snapshot())->revision == 3 && notifications == 3 && direction == TableSortOrder::ascending,
            "Keyboard sorting did not skip a disabled header or activate the selected column");
    input.source_revision = 3; input.sorter = std::make_shared<NumericSorter>(); good(table->set_options(input));
    good(table->sort("value", TableSortOrder::ascending)); current = take(source->snapshot());
    require(current->revision == 4 && current->rows[0].key == "r3" && current->rows[1].key == "r0" &&
            current->rows[2].key == "r2" && current->rows[3].key == "r1" && notifications == 4,
            "A replaceable numeric sorting policy was not applied");
    input.source_revision = 4; input.sorter = std::make_shared<InvalidSorter>(); good(table->set_options(input));
    require(!table->sort("value", TableSortOrder::ascending) && take(source->snapshot())->revision == 4 && notifications == 4,
            "An invalid table sorting policy changed the source");
    input.sorter.reset(); input.sorting_enabled = false; good(table->set_options(input));
    require(!table->sort("value", TableSortOrder::ascending), "Disabled table sorting was accepted");
    input.sorting_enabled = true; auto publishing = std::make_shared<PublishingEditor>(); publishing->editor = order;
    input.order_editor = publishing; good(table->set_options(input));
    publishing->changed = [&] { input.source_revision = 5; good(table->set_options(input)); };
    good(table->sort("value", TableSortOrder::descending));
    require(take(source->snapshot())->revision == 5 && notifications == 5 && direction == TableSortOrder::descending,
            "A reactive ordering update hid a successful sort callback");
    (void)take(source->edit({.revision=5, .cells={{"r1", "value", "11"}}})); input.source_revision = 6;
    good(table->set_options(input));
    auto drawing = take(tree->drawing_snapshot({240, 150}));
    require(!has_text(drawing, "↑") && !has_text(drawing, "↓"), "External table changes left a misleading sort indicator");
    auto changing = std::make_shared<ChangingSorter>(); changing->source = source; input.sorter = changing;
    input.order_editor = order; good(table->set_options(input)); const auto before = take(source->snapshot())->rows[0].key;
    require(!table->sort("value", TableSortOrder::ascending) && take(source->snapshot())->revision == 7 &&
            take(source->snapshot())->rows[0].key == before && notifications == 5,
            "A source change during sorting partially reordered the table");
    good(host.clear_content()); require(!table->sort("value", TableSortOrder::ascending), "A retired table accepted a sort");
    tree->close();

    auto narrow_source = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="a", .width=100}, {.key="b", .width=100, .enabled=false}, {.key="c", .width=100}},
        std::vector<lvui::TableRow>{{.key="one", .cells={{.text="same"}, {}, {.text="z"}}},
                                   {.key="two", .cells={{.text="same"}, {}, {.text="a"}}}});
    lvui::native::TableInput narrow; narrow.controller = std::make_shared<lvui::TableController>(narrow_source);
    narrow.order_editor = std::make_shared<lvui::TableMemoryOrderEditor>(narrow_source); narrow.sorting_enabled = true;
    narrow.appearance.width = 120; narrow.appearance.height = 100;
    auto narrow_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto narrow_host = take(narrow_tree->content_host({}, "narrow-host", {130, 110}));
    std::optional<lvui::native::NativeTable> narrow_table; std::size_t keyboard_column = 99;
    good(narrow_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        auto value = lvui::native::NativeTable::create(builder, *stack, "narrow", narrow,
            {.sorted = [&](std::size_t column_value, TableSortOrder) { keyboard_column = column_value; }});
        if (!value) return std::unexpected(value.error()); narrow_table = *value; return {};
    }));
    (void)take(narrow_table->frame());
    good(narrow_tree->input({.kind=WidgetInputKind::pointer_down, .x=20, .y=10, .buttons=1}));
    good(narrow_tree->input({.kind=WidgetInputKind::pointer_up, .x=20, .y=10}));
    good(narrow_tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::right}));
    require(take(narrow_table->frame())->x > 0, "Header keyboard navigation did not reveal an off-screen column");
    good(narrow_tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::enter}));
    require(keyboard_column == 2 && take(narrow_source->snapshot())->rows[0].key == "two",
            "Header keyboard navigation did not skip the disabled column or sort the revealed column");
    narrow_tree->close();
}
