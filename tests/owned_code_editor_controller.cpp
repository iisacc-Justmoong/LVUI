#include <LVUI/CodeEditorController.h>

#include <memory>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class ReentrantEditor final : public lvui::CodeDocumentEditor {
public:
    std::shared_ptr<lvui::CodeMemoryDocument> document; lvui::CodeEditorController* controller = nullptr; bool guarded = false;
    lvui::CodeDocumentResult<lvui::CodeDocumentChange> replace(const lvui::CodeDocumentEdit& edit) override {
        auto nested = controller->replace_selection("nested"); auto selection = controller->set_selection(0, 0); auto refresh = controller->refresh();
        guarded = !nested && nested.error().code == lvui::CodeDocumentErrorCode::busy && !selection &&
            selection.error().code == lvui::CodeDocumentErrorCode::busy && !refresh && refresh.error().code == lvui::CodeDocumentErrorCode::busy;
        return document->replace(edit);
    }
};
}
int main() {
    using namespace lvui;
    auto document = take(CodeMemoryDocument::create("a😀b\nc"));
    auto readonly = take(CodeEditorController::create(document));
    require(!readonly.replace_selection("x") && !CodeEditorController::create({}), "Read-only or missing code source accepted an edit");
    auto controller = take(CodeEditorController::create(document, document));
    require(take(controller.set_selection(3, 3)) && !controller.set_selection(2, 2) &&
            take(controller.move(CodeMove::previous_scalar)) && take(controller.selection()).caret == 1 &&
            take(controller.move(CodeMove::next_scalar)) && take(controller.selection()).caret == 3 &&
            take(document->snapshot())->revision() == 0, "Code navigation split a scalar or edited the document");
    require(take(controller.move(CodeMove::document_end, true)) && take(controller.selection()).range().length == 3 &&
            take(controller.move(CodeMove::previous_scalar)) && take(controller.selection()).caret == 3,
            "Code selection extension or collapse failed");
    require(take(controller.set_selection(1, 3)) && take(controller.replace_selection("X")).revision == 1 &&
            take(controller.selection()) == CodeSelection{2, 2} && take(take(controller.snapshot())->read({0, 5}, 32)) == "aXb\nc",
            "Code selection replacement did not update source and caret together");
    (void)take(document->replace({.revision=1, .range={0, 1}, .text="external"}));
    auto stale = controller.replace_selection("mine");
    require(!stale && stale.error().code == CodeDocumentErrorCode::stale && take(controller.selection()).caret == 2 &&
            take(document->snapshot())->revision() == 2, "A stale code edit overwrote an external change or moved the caret");
    require(take(controller.refresh()) && take(controller.snapshot())->revision() == 2 &&
            take(controller.replace_selection("!")).revision == 3, "Code controller did not recover after refreshing a stale source");
    auto reentrant = std::make_shared<ReentrantEditor>(); reentrant->document = document;
    auto guarded = take(CodeEditorController::create(document, reentrant)); reentrant->controller = &guarded;
    require(take(guarded.replace_selection("Z")).revision == 4 && reentrant->guarded,
            "A reentrant code editor changed the active edit or selection");
    bool wrong_thread = false;
    std::thread worker([&] { auto result = guarded.set_selection(0, 0); wrong_thread = !result && result.error().code == CodeDocumentErrorCode::wrong_thread; });
    worker.join(); require(wrong_thread, "Code controller accepted a foreign-thread update");
    auto shrink = take(CodeEditorController::create(document, document));
    const auto end = take(shrink.snapshot())->length(); require(take(shrink.set_selection(end, end)), "Code selection setup failed");
    (void)take(document->replace({.revision=4, .range={0, end}, .text="😀"}));
    require(take(shrink.refresh()) && take(shrink.selection()).caret == 2, "External code shrink did not clamp the view selection");

    auto lines = take(CodeMemoryDocument::create("ab😀c\r\nx\nuv😀z\n"));
    auto navigation = take(CodeEditorController::create(lines, lines));
    require(take(navigation.set_selection(5, 5)) && take(navigation.move(CodeMove::line_end)) == false &&
            take(navigation.move(CodeMove::next_line)) && take(navigation.selection()).caret == 8 &&
            take(navigation.move(CodeMove::next_line)) && take(navigation.selection()).caret == 14 &&
            take(navigation.move(CodeMove::previous_line, true)) && take(navigation.selection()) == CodeSelection{14, 8},
            "Vertical code movement did not retain the logical column across short and Unicode lines");
    require(take(navigation.move(CodeMove::line_start)) && take(navigation.selection()).caret == 7 &&
            take(navigation.move(CodeMove::line_end)) && take(navigation.selection()).caret == 8 &&
            take(navigation.move(CodeMove::document_end)) && take(navigation.selection()).caret == 15 &&
            take(navigation.move(CodeMove::line_start)) == false,
            "Line navigation did not exclude CRLF or recognize the final empty line");
    auto deletion = take(CodeMemoryDocument::create("a😀b"));
    auto editing = take(CodeEditorController::create(deletion, deletion));
    require(take(editing.set_selection(3, 3)) && take(editing.erase(CodeErase::previous_scalar)).has_value() &&
            take(take(editing.snapshot())->read({0, 2}, 8)) == "ab" && take(editing.selection()).caret == 1 &&
            take(editing.erase(CodeErase::next_scalar)).has_value() && take(editing.selection()).caret == 1,
            "Scalar deletion split an emoji or moved the caret incorrectly");
    require(!take(editing.erase(CodeErase::next_scalar)).has_value() && take(editing.snapshot())->revision() == 2,
            "Deleting beyond the document changed its revision");
    auto explicit_range = editing.replace_range({0, 1}, "😀");
    require(explicit_range && take(editing.selection()).caret == 2 && take(editing.snapshot())->length() == 2,
            "Explicit code replacement did not preserve UTF-16 positions");
    const auto unchanged = take(editing.selection());
    require(!editing.replace_range({1, 0}, "x") && take(editing.selection()) == unchanged,
            "An invalid replacement range changed the selection");
    (void)take(deletion->replace({.revision=3, .range={0, 0}, .text="external"}));
    auto stale_erase = editing.erase(CodeErase::previous_scalar);
    require(!stale_erase && stale_erase.error().code == CodeDocumentErrorCode::stale && take(editing.selection()) == unchanged,
            "A stale deletion changed the view selection");
}
