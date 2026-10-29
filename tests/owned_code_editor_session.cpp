#include <LVUI/CodeEditorSession.h>

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
    std::shared_ptr<lvui::CodeMemoryDocument> document; lvui::CodeEditorSession* session = nullptr; bool guarded = false;
    lvui::CodeDocumentResult<lvui::CodeDocumentChange> replace(const lvui::CodeDocumentEdit& edit) override {
        auto nested = session->set_composition("nested");
        guarded = !nested && nested.error().code == lvui::CodeDocumentErrorCode::busy;
        return document->replace(edit);
    }
};
}
int main() {
    using namespace lvui;
    auto document = take(CodeMemoryDocument::create("A😀B"));
    auto readonly = take(CodeEditorSession::create(document));
    require(!readonly.set_composition("x") && !readonly.insert("x"), "Read-only session accepted editing");
    auto session = take(CodeEditorSession::create(document, document));
    require(take(session.set_selection(1, 3)) && take(session.set_composition("ㅎ")) &&
            take(session.view())->length() == 3 && take(take(session.view())->read({0, 3}, 16)) == "AㅎB" &&
            take(session.selection()) == CodeSelection{2, 2} && take(document->snapshot())->revision() == 0,
            "Composition did not preserve the source and project its selection");
    require(!session.set_selection(0, 0) && !session.move(CodeMove::next_scalar) &&
            !session.set_composition("x", CodeDocumentRange{0, 1}), "An unrelated action changed an active composition");
    require(take(session.set_composition("한", CodeDocumentRange{1, 1}, CodeDocumentRange{0, 1})) &&
            take(session.selection()) == CodeSelection{1, 2} && take(session.marked())->location == 1 &&
            take(document->snapshot())->revision() == 0, "Composition update did not replace the prior marked text");
    auto committed = take(session.commit_composition());
    require(committed && committed->revision == 1 && !take(session.marked()) &&
            take(take(session.view())->read({0, 3}, 16)) == "A한B", "Composition did not commit exactly once");
    require(!take(session.commit_composition()), "An absent composition committed again");
    require(take(session.set_selection(1, 2)) && take(session.set_composition("😀")), "Could not start a second composition");
    (void)take(document->replace({.revision=1, .range={0, 0}, .text="external"}));
    auto stale = session.commit_composition();
    require(!stale && stale.error().code == CodeDocumentErrorCode::stale && take(session.marked()).has_value() &&
            take(document->snapshot())->revision() == 2, "A stale composition overwrote the source or discarded user input");
    require(take(session.cancel_composition()) && take(session.refresh()) && !take(session.marked()),
            "A cancelled stale composition did not allow refresh");
    require(take(session.set_composition("x")) && take(session.set_composition("")) && !take(session.marked()),
            "An empty marked string did not cancel composition");
    require(take(session.insert("!", CodeDocumentRange{0, 0})).revision == 3 && take(session.erase(CodeErase::previous_scalar)).has_value(),
            "Session text replacement or deletion did not reach its editor");
    bool wrong_thread = false; std::thread worker([&] { auto view = session.view(); wrong_thread = !view && view.error().code == CodeDocumentErrorCode::wrong_thread; });
    worker.join(); require(wrong_thread, "Session accepted a foreign-thread view request");
    auto editor = std::make_shared<ReentrantEditor>(); editor->document = document;
    auto guarded = take(CodeEditorSession::create(document, editor)); editor->session = &guarded;
    (void)take(guarded.insert("x")); require(editor->guarded, "A reentrant editor changed the composition during commit");
}
