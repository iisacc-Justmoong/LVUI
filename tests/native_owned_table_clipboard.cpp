#include <LVUI/NativeTable.h>
#include <iiGUI/WidgetState.h>

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
class PublishingEditor final : public lvui::TableEditor {
public:
    std::shared_ptr<lvui::TableMemory> source; std::function<void()> changed;
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& edit) override {
        auto result = source->edit(edit); if (result && changed) changed(); return result;
    }
};
}

int main() {
    using namespace ii::gui;
    auto source = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="a", .width=80}, {.key="b", .width=80}, {.key="c", .width=80}},
        std::vector<lvui::TableRow>{{.key="r0", .cells={{.text="a"}, {.text="b"}, {.text="c"}}},
                                   {.key="r1", .cells={{.text="d"}, {.text="e"}, {.text="f"}}},
                                   {.key="r2", .cells={{.text="g"}, {.text="h"}, {.text="locked", .editable=false}}}});
    lvui::native::TableInput input; input.controller = std::make_shared<lvui::TableController>(source);
    input.editor = source; input.editable = true; input.appearance.width = 260; input.appearance.height = 110;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {280, 130}));
    std::optional<lvui::native::NativeTable> table; std::optional<lvui::TableRange> changed;
    std::string buffer; int notifications = 0;
    bool fail_write = false, reenter = true, blocked = false, retire_on_read = false, mutate_on_write = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto widget = lvui::native::NativeTable::create(builder, *column, "table", input,
            {.range_values_changed = [&](const auto& value) { changed = value; ++notifications; }},
            {.read = [&]() -> ii::core::Result<std::string> { if (retire_on_read) good(host.clear_content()); return buffer; },
             .write = [&](std::string value) -> ii::core::Result<void> {
                 if (reenter) { auto nested = table->copy_selection(); blocked = !nested && nested.error().code == ii::core::ErrorCode::busy; }
                 if (fail_write) return ii::core::fail(ii::core::ErrorCode::failed, "Clipboard rejected write");
                 buffer = std::move(value);
                 if (mutate_on_write) (void)take(source->edit({.revision=3, .cells={{"r0", "a", "external"}}}));
                 return {};
             }});
        if (!widget) return std::unexpected(widget.error()); table = *widget; return {};
    }));
    (void)take(table->frame());
    const auto send = [&](WidgetInputKind kind, double x, double y) { good(tree->input({.kind=kind, .x=x, .y=y, .buttons=1})); };
    const auto click = [&](double x, double y) { send(WidgetInputKind::pointer_down, x, y); send(WidgetInputKind::pointer_up, x, y); };
    send(WidgetInputKind::pointer_down, 30, 34); send(WidgetInputKind::pointer_move, 100, 58); send(WidgetInputKind::pointer_up, 100, 58);
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::copy, .meta=true}));
    require(buffer == "a\tb\nd\te" && blocked && take(source->snapshot())->revision == 0 && notifications == 0,
            "Copy did not preserve the range, document or reentrancy guard");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::cut}));
    auto current = take(source->snapshot());
    require(current->revision == 1 && current->rows[0].cells[0].text.empty() && current->rows[1].cells[1].text.empty() &&
            changed && changed->first_row == 0 && changed->last_row == 1 && changed->last_column == 1 && notifications == 1,
            "Cut did not publish one atomic range change");
    auto content = std::make_shared<const ClipboardContent>(ClipboardContent{.text=buffer});
    good(tree->input({.kind=WidgetInputKind::text, .content=content})); current = take(source->snapshot());
    require(current->revision == 2 && current->rows[0].cells[0].text == "a" && current->rows[1].cells[1].text == "e" && notifications == 2,
            "Clipboard content did not restore the selected range");
    buffer = "fill"; good(table->paste_clipboard()); current = take(source->snapshot());
    require(current->revision == 3 && current->rows[0].cells[0].text == "fill" && current->rows[1].cells[1].text == "fill" && notifications == 3,
            "Programmatic paste did not use the replaceable clipboard or fill the range");
    good(table->paste_text("fill")); require(take(source->snapshot())->revision == 3 && notifications == 3,
            "A no-op table paste published a range change");
    fail_write = true; require(!table->cut_selection() && take(source->snapshot())->revision == 3 && notifications == 3,
            "A failed clipboard write partially cut the table"); fail_write = false;
    mutate_on_write = true;
    require(!table->cut_selection() && take(source->snapshot())->revision == 4 &&
            take(source->snapshot())->rows[0].cells[0].text == "external" &&
            take(source->snapshot())->rows[1].cells[1].text == "fill" && notifications == 3,
            "A concurrent clipboard write partially cut newer table data");
    mutate_on_write = false;
    require(!table->paste_text("stale") && take(source->snapshot())->revision == 4 && notifications == 3,
            "A stale clipboard event overwrote newer table data");
    good(table->paste_text("fresh")); require(take(source->snapshot())->revision == 5 && notifications == 4,
            "Table clipboard did not recover after refreshing stale data");
    click(200, 82); buffer = "preserved";
    require(!table->paste_text("no") && !table->cut_selection() && buffer == "preserved" && take(source->snapshot())->revision == 5,
            "A read-only table cell was changed or cut to the clipboard");
    click(20, 34); input.clipboard_enabled = false; input.source_revision = 5; good(table->set_options(input));
    require(!table->copy_selection() && !table->paste_text("no") && take(source->snapshot())->revision == 5,
            "Disabled table clipboard operations were accepted");
    input.clipboard_enabled = true; good(table->set_options(input)); good(table->begin_edit());
    require(!table->paste_text("no") && !table->copy_selection(), "Clipboard interfered with an active cell editor");
    good(table->cancel_edit());
    auto publishing = std::make_shared<PublishingEditor>(); publishing->source = source; input.editor = publishing;
    good(table->set_options(input)); publishing->changed = [&] { input.source_revision = 6; good(table->set_options(input)); };
    good(table->paste_text("published")); require(take(source->snapshot())->revision == 6 && notifications == 5,
            "A reactive editor update hid a successfully committed clipboard change");
    retire_on_read = true;
    require(!table->paste_clipboard() && take(source->snapshot())->revision == 6 && notifications == 5,
            "Retirement during clipboard read edited data or delivered a stale callback");
    tree->close();
}
