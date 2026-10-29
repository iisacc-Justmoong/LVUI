#include <LVUI/NativeCodeEditor.h>
#include <iiGUI/WidgetState.h>

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
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}
int main() {
    using namespace ii::gui; using namespace lvui;
    auto document = take(CodeMemoryDocument::create("A😀\n두번째"));
    native::CodeEditorInput input; input.source = document; input.editor = document; input.appearance.width = 240; input.appearance.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 120}));
    std::optional<native::NativeCodeEditor> editor; std::vector<CodeDocumentChange> edits; std::string clipboard;
    bool checked_reentrancy = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = native::NativeCodeEditor::create(builder, *column, "editor", input,
            {.edited = [&](const auto& change) { edits.push_back(change);
                good(editor->sync_source()); auto nested = editor->set_selection(0, 0);
                checked_reentrancy = !nested && nested.error().code == ii::core::ErrorCode::busy; }},
            {.write = [&](std::string text) -> ii::core::Result<void> { clipboard = std::move(text); return {}; }});
        if (!value) return std::unexpected(value.error()); editor = *value; return {};
    }));
    require(take(editor->frame()).lines.size() == 2 && take(editor->root().snapshot()).size.width == 240,
            "Native code editor did not render its initial visible lines");
    auto drawing = take(tree->drawing_snapshot({300, 120}));
    require(!drawing.images.empty() || !drawing.overlays.empty(), "Native code editor produced no shaped text drawing");
    auto appearance = take(editor->appearance()); appearance.width = 210; good(editor->set_appearance(appearance));
    require(take(editor->root().snapshot()).size.width == 210, "Code editor did not apply a changed appearance");
    auto invalid = appearance; invalid.line_height = 0;
    require(!editor->set_appearance(invalid) && take(editor->appearance()) == appearance,
            "An invalid appearance replaced the working code editor");
    good(editor->root().focus()); good(editor->set_selection(1, 3));
    good(tree->input({.kind=WidgetInputKind::composition, .text="ㅎ", .composition_selection=TextRange{1, 0}}));
    require(take(document->snapshot())->revision() == 0 && take(editor->snapshot()).marked.has_value() &&
            take(editor->root().text_content({0, 2})).text == "Aㅎ", "Native code composition changed the document or exposed stale text");
    good(tree->input({.kind=WidgetInputKind::composition, .text="한", .composition_selection=TextRange{1, 0}}));
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(document->snapshot())->revision() == 1 && edits.size() == 1 && !take(editor->snapshot()).marked &&
            take(take(document->snapshot())->read({0, 3}, 16)) == "A한\n", "Native composition did not commit once");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::backspace}));
    good(tree->input({.kind=WidgetInputKind::text, .text="B"}));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3 && checked_reentrancy &&
            take(take(document->snapshot())->read({0, 3}, 16)) == "AB\n", "Native keyboard editing did not reach the independent source");
    good(editor->insert(""));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3, "An empty code insertion emitted a false edit");
    good(editor->set_selection(0, 2)); good(editor->insert("AB"));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3 &&
            take(editor->snapshot()).selection == CodeSelection{2, 2}, "Identical replacement emitted an edit or failed to collapse selection");
    good(editor->set_selection(0, 2)); good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::copy}));
    require(clipboard == "AB" && take(document->snapshot())->revision() == 3, "Code copy changed the document or lost selected text");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::cut}));
    require(take(document->snapshot())->revision() == 4 && clipboard == "AB", "Code cut did not commit the copied selection");
    good(editor->insert("AB"));
    (void)take(document->replace({.revision=5, .range={0, 0}, .text="external "}));
    require(!editor->insert("stale") && take(document->snapshot())->revision() == 6, "A stale native editor overwrote an external change");
    good(editor->refresh()); good(editor->insert("!")); require(take(document->snapshot())->revision() == 7, "Native editor did not recover after refresh");
    bool wrong_thread = false; std::thread worker([&] { auto value = editor->set_selection(0, 0); wrong_thread = !value && value.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "Native code editor accepted a foreign-thread mutation");
    good(host.clear_content()); require(!editor->set_selection(0, 0), "A retired code editor accepted an update"); tree->close();

    std::string lines; for (int i = 0; i < 10000; ++i) lines += "line " + std::to_string(i) + "\n";
    auto large = take(CodeMemoryDocument::create(std::move(lines))); native::CodeEditorInput many;
    many.source = large; many.appearance.width = 180; many.appearance.height = 80;
    auto next_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto next_host = take(next_tree->content_host({}, "host", {200, 100})); std::optional<native::NativeCodeEditor> reader;
    good(next_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = native::NativeCodeEditor::create(builder, *column, "reader", many);
        if (!value) return std::unexpected(value.error()); reader = *value; return {};
    }));
    good(reader->scroll_to_line(9000)); const auto frame = take(reader->frame());
    require(!frame.lines.empty() && frame.lines.front().line == 9000 && frame.lines.size() <= 7 &&
            !reader->insert("x") && take(large->snapshot())->revision() == 0,
            "Read-only large code view materialized unrelated lines or edited the source");
    (void)take(next_tree->drawing_snapshot({200, 100})); good(next_host.clear_content()); next_tree->close();
}
