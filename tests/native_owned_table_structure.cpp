#include <LVUI/NativeTable.h>
#include <iiGUI/WidgetState.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
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
class PublishingEditor final : public lvui::TableStructureEditor {
public:
    std::shared_ptr<lvui::TableStructureEditor> editor; std::function<void()> changed;
    lvui::TableResult<std::uint64_t> apply(const lvui::TableStructureEdit& edit) override {
        auto result = editor->apply(edit); if (result && changed) changed(); return result;
    }
};
}
int main() {
    using namespace ii::gui;
    auto source = std::make_shared<lvui::TableMemory>(std::vector<lvui::TableColumn>{{.key="a", .width=80}, {.key="b", .width=80}},
        std::vector<lvui::TableRow>{{.key="r0", .cells={{.text="A"}, {.text="B"}}}, {.key="r1", .cells={{.text="C"}, {.text="D"}}}});
    auto editor = std::make_shared<lvui::TableMemoryStructureEditor>(source);
    lvui::native::TableInput input; input.controller = std::make_shared<lvui::TableController>(source); input.structure_editor = editor;
    input.column_resize_enabled = input.row_resize_enabled = true; input.appearance.width = 200; input.appearance.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 130})); std::optional<lvui::native::NativeTable> table;
    int changes = 0, column_changes = 0, row_changes = 0; std::size_t last_index = 99; double last_size = 0;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading}); if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeTable::create(builder, *column, "table", input,
            {.structure_changed = [&](const auto&) { ++changes; },
             .column_resized = [&](std::size_t index, double width) { ++column_changes; last_index = index; last_size = width; },
             .row_resized = [&](std::size_t index, double height) { ++row_changes; last_index = index; last_size = height; }});
        if (!value) return std::unexpected(value.error()); table = *value; return {};
    }));
    (void)take(table->frame());
    const auto send = [&](WidgetInputKind kind, double x, double y) { return tree->input({.kind=kind, .x=x, .y=y, .buttons=1}); };
    good(send(WidgetInputKind::pointer_down, 78, 10)); good(send(WidgetInputKind::pointer_move, 108, 10));
    require(take(source->snapshot())->revision == 0 && take(source->snapshot())->columns[0].width == 80,
            "Column resize preview changed document data");
    good(send(WidgetInputKind::pointer_up, 108, 10));
    require(take(source->snapshot())->revision == 1 && take(source->snapshot())->columns[0].width == 110 &&
            changes == 1 && column_changes == 1 && last_index == 0 && last_size == 110,
            "Column resizing did not commit one dimension and callback");
    good(send(WidgetInputKind::pointer_down, 40, 46)); good(send(WidgetInputKind::pointer_move, 40, 66));
    require(take(source->snapshot())->revision == 1 && take(source->snapshot())->rows[0].height == 24,
            "Row resize preview changed document data");
    good(send(WidgetInputKind::pointer_up, 40, 66));
    require(take(source->snapshot())->revision == 2 && take(source->snapshot())->rows[0].height == 44 &&
            changes == 2 && row_changes == 1 && last_index == 0 && last_size == 44,
            "Row resizing did not commit one dimension and callback");
    good(send(WidgetInputKind::pointer_down, 108, 10)); good(send(WidgetInputKind::pointer_move, 125, 10));
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::escape})); good(send(WidgetInputKind::pointer_up, 125, 10));
    require(take(source->snapshot())->revision == 2 && changes == 2, "Escape did not cancel the resize preview");
    good(send(WidgetInputKind::pointer_down, 108, 10)); good(send(WidgetInputKind::pointer_move, 125, 10));
    input.source_revision = 2; input.appearance.width = 210; good(table->set_options(input)); good(send(WidgetInputKind::pointer_up, 125, 10));
    require(take(source->snapshot())->revision == 2 && changes == 2, "A reactive change did not cancel an in-progress resize");
    good(send(WidgetInputKind::pointer_down, 108, 10)); good(send(WidgetInputKind::pointer_move, 125, 10));
    (void)take(editor->apply({.revision=2, .operations={lvui::TableResizeRows{{{"r1", 30}}}}}));
    require(!send(WidgetInputKind::pointer_up, 125, 10) && take(source->snapshot())->revision == 3 &&
            take(source->snapshot())->columns[0].width == 110 && changes == 2,
            "A stale resize overwrote an external table change");
    lvui::TableStructureEdit insert{.revision=3, .operations={lvui::TableInsertRows{.index=1, .rows={{.key="r2", .cells={{.text="X"}, {.text="Y"}}}}}}};
    require(take(table->apply_structure(insert)) == 4 && take(source->snapshot())->rows[1].key == "r2" && changes == 3,
            "Programmatic table structure edit did not commit atomically");
    require(!table->apply_structure({.revision=4, .operations={lvui::TableRemoveRows{{"r0", "r0"}}}}) &&
            take(source->snapshot())->revision == 4 && changes == 3, "An invalid structure request partially changed the source");
    auto publishing = std::make_shared<PublishingEditor>(); publishing->editor = editor; input.source_revision = 4; input.structure_editor = publishing;
    good(table->set_options(input)); publishing->changed = [&] { input.source_revision = 5; good(table->set_options(input)); };
    good(send(WidgetInputKind::pointer_down, 108, 10)); good(send(WidgetInputKind::pointer_up, 118, 10));
    require(take(source->snapshot())->revision == 5 && changes == 4 && column_changes == 2 && last_size == 120,
            "A reactive structure editor suppressed a committed resize callback");
    bool wrong_thread = false; std::thread worker([&] { auto result = table->apply_structure({.revision=5});
        wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "Table structure host accepted a foreign thread");
    good(host.clear_content()); require(!table->apply_structure({.revision=5}), "A retired table accepted a structure edit"); tree->close();
}
