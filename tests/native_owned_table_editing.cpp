#include <LVUI/NativeTable.h>
#include <iiGUI/WidgetState.h>

#include <charconv>
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
class IntegerEditor final : public lvui::TableEditor {
public:
    explicit IntegerEditor(std::shared_ptr<lvui::TableMemory> source) : source_(std::move(source)) {}
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& edit) override {
        int value = 0; const auto& text = edit.cells.front().text;
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Expected an integer"});
        return source_->edit(edit);
    }
private:
    std::shared_ptr<lvui::TableMemory> source_;
};
class RetiringEditor final : public lvui::TableEditor {
public:
    std::shared_ptr<lvui::TableMemory> source; ii::gui::Widget host;
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& value) override {
        auto result = source->edit(value); if (result) good(host.clear_content()); return result;
    }
};
}
int main() {
    using namespace ii::gui;
    std::vector<lvui::TableColumn> columns{{.key="amount", .title="Amount", .width=80}, {.key="locked", .title="Locked", .width=80}};
    std::vector<lvui::TableRow> rows;
    for (int i = 0; i < 30; ++i) rows.push_back({.key="r" + std::to_string(i),
        .cells={{.text="10", .value_type="integer"}, {.text="No", .editable=false}}});
    auto source = std::make_shared<lvui::TableMemory>(columns, rows);
    lvui::native::TableInput input; input.controller = std::make_shared<lvui::TableController>(source);
    input.editor = std::make_shared<IntegerEditor>(source); input.editable = true;
    input.appearance.width = 200; input.appearance.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 130}));
    std::optional<lvui::native::NativeTable> table; std::vector<std::string> drafts, submitted, rejected;
    std::optional<lvui::native::TableCellEvent> last_event;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeTable::create(builder, *column, "table", input,
            {.edited = [&](const auto& event) { drafts.push_back(event.text); last_event = event; },
             .submitted = [&](const auto& event) { submitted.push_back(event.text); },
             .rejected = [&](const auto& event, const auto&) { rejected.push_back(event.text); }});
        if (!value) return std::unexpected(value.error()); table = *value; return {};
    }));
    (void)take(table->frame());
    const auto click = [&](double x, double y) { good(tree->input({.kind=WidgetInputKind::pointer_down, .x=x, .y=y, .buttons=1}));
        good(tree->input({.kind=WidgetInputKind::pointer_up, .x=x, .y=y})); };
    const auto key = [&](WidgetKey value) { good(tree->input({.kind=WidgetInputKind::key_down, .key=value})); };
    const auto replace = [&](std::string text) { auto field = take(table->editor()); auto state = take(field.snapshot());
        good(field.set_selection(0, state.text.size())); good(tree->input({.kind=WidgetInputKind::text, .text=std::move(text)})); };
    click(40, 34); key(WidgetKey::enter); require(bool(table->editor()), "Enter did not open a table editor");
    replace("bad"); require(drafts.back() == "bad" && last_event && last_event->position.row == "r0" &&
            last_event->position.column == "amount" && last_event->row_index == 0 && last_event->column_index == 0 &&
            last_event->value_type == "integer" && take(source->snapshot())->revision == 0, "Typing lost its context or edited the source document");
    key(WidgetKey::enter); require(rejected == std::vector<std::string>{"bad"} && take(table->editor().value().snapshot()).text == "bad" &&
            take(source->snapshot())->revision == 0, "Rejected table edit lost its draft or changed source data");
    replace("12"); key(WidgetKey::enter);
    require(submitted == std::vector<std::string>{"12"} && !table->editor() && take(source->snapshot())->revision == 1 &&
            take(source->snapshot())->rows[0].cells[0].text == "12", "Valid table edit was not committed exactly once");
    good(table->begin_edit()); good(take(table->editor()).set_text("16")); good(table->commit_edit());
    require(take(source->snapshot())->revision == 2 && take(source->snapshot())->rows[0].cells[0].text == "16" &&
            submitted == std::vector<std::string>{"12", "16"}, "Programmatic editor text did not reach the committed draft");
    key(WidgetKey::enter); replace("13"); (void)take(source->edit({.revision=2, .cells={{"r0", "amount", "14"}}}));
    input.source_revision = 3; good(table->set_options(input));
    require(take(table->editor().value().snapshot()).text == "13", "Reactive source refresh discarded the active draft");
    key(WidgetKey::enter); require(rejected.back() == "13" && take(table->editor().value().snapshot()).text == "13" &&
            take(source->snapshot())->rows[0].cells[0].text == "14", "Stale table edit overwrote newer data or lost its draft");
    key(WidgetKey::escape); require(!table->editor() && !take(table->failure()) && take(source->snapshot())->revision == 3,
            "Escape did not cancel a conflicted draft and clear its failure");
    click(40, 34); key(WidgetKey::enter); replace("99"); click(120, 34);
    require(!table->editor() && take(source->snapshot())->revision == 3, "Clicking elsewhere did not cancel the draft");
    require(!table->begin_edit(), "A read-only table cell entered edit mode");
    key(WidgetKey::enter); require(!table->editor(), "Enter opened a read-only table cell");
    click(40, 34); good(table->begin_edit()); replace("100"); good(table->scroll().set_scroll_offset(500));
    require(!table->editor() && take(source->snapshot())->revision == 3, "Scrolling an edited cell away did not cancel its draft");
    good(table->scroll().set_scroll_offset(0)); click(40, 34);
    auto retiring = std::make_shared<RetiringEditor>(); retiring->source = source; retiring->host = host;
    input.source_revision = 3; input.editor = retiring; good(table->set_options(input));
    good(table->begin_edit()); replace("15"); key(WidgetKey::enter);
    require(take(source->snapshot())->revision == 4 && submitted == std::vector<std::string>{"12", "16"} && !table->set_options(input),
            "A retiring editor delivered a stale submission callback or retained the table");
    tree->close();
}
