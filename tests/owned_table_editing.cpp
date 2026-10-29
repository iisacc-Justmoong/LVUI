#include <LVUI/TableEditing.h>

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
class ReentrantEditor final : public lvui::TableEditor {
public:
    std::shared_ptr<lvui::TableMemory> source; lvui::TableEditing* active = nullptr; bool guarded = false;
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& value) override {
        auto nested = active->commit(); auto changed = active->set_draft("99"); auto canceled = active->cancel();
        guarded = !nested && nested.error().code == lvui::TableErrorCode::busy && !changed &&
                  changed.error().code == lvui::TableErrorCode::busy && !canceled && canceled.error().code == lvui::TableErrorCode::busy;
        return source->edit(value);
    }
};
class IntegerEditor final : public lvui::TableEditor {
public:
    explicit IntegerEditor(std::shared_ptr<lvui::TableMemory> source) : source_(std::move(source)) {}
    lvui::TableResult<std::uint64_t> edit(const lvui::TableEdit& edit) override {
        int parsed = 0; const auto& text = edit.cells.front().text;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
            return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Expected an integer"});
        return source_->edit(edit);
    }
private:
    std::shared_ptr<lvui::TableMemory> source_;
};
}
int main() {
    using namespace lvui;
    auto memory = std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="value", .title="Value"}},
        std::vector<TableRow>{{.key="row", .cells={{.text="10", .value_type="integer"}}},
                              {.key="locked", .cells={{.text="x", .editable=false}}}});
    auto editor = std::make_shared<IntegerEditor>(memory); auto original = take(memory->snapshot());
    require(!TableEditing::begin(original, editor, {"locked", "value"}) &&
            !TableEditing::begin(original, editor, {"missing", "value"}) &&
            !TableEditing::begin(original, {}, {"row", "value"}), "Invalid table edit session was accepted");
    auto session = take(TableEditing::begin(original, editor, {"row", "value"}));
    require(take(session.state()).original == "10" && take(session.state()).value_type == "integer" &&
            take(session.set_draft("invalid")) && take(memory->snapshot())->revision == 0,
            "Table draft changed source data or lost its metadata");
    auto rejected = session.commit(); require(!rejected && take(session.state()).draft == "invalid" &&
            take(memory->snapshot())->revision == 0, "Rejected edit lost its draft or changed the document");
    require(take(session.set_draft("12")) && !take(session.set_draft("12")) && take(session.commit()) == 1 && !session.commit() &&
            take(memory->snapshot())->rows[0].cells[0].text == "12" && original->rows[0].cells[0].text == "10",
            "Table edit did not commit exactly once or preserve its original snapshot");
    auto stale = take(TableEditing::begin(take(memory->snapshot()), editor, {"row", "value"}));
    require(take(stale.set_draft("13")) && take(memory->edit({.revision=1, .cells={{"row", "value", "14"}}})) == 2,
            "Concurrent edit setup failed");
    auto conflict = stale.commit(); require(!conflict && conflict.error().code == TableErrorCode::stale &&
            take(stale.state()).draft == "13" && take(memory->snapshot())->rows[0].cells[0].text == "14",
            "Stale edit overwrote newer data or lost its draft");
    require(bool(stale.cancel()) && !stale.state() && !stale.commit(), "Canceled table edit remained active");
    auto isolated = take(TableEditing::begin(take(memory->snapshot()), editor, {"row", "value"}));
    bool wrong_thread = false;
    std::thread worker([&] { auto result = isolated.set_draft("20"); wrong_thread = !result && result.error().code == TableErrorCode::wrong_thread; });
    worker.join(); require(wrong_thread && !isolated.set_draft(std::string("bad\xff", 4)), "Table edit accepted a foreign thread or invalid UTF-8");
    auto reentrant = std::make_shared<ReentrantEditor>(); reentrant->source = memory;
    auto guarded = take(TableEditing::begin(take(memory->snapshot()), reentrant, {"row", "value"})); reentrant->active = &guarded;
    require(take(guarded.set_draft("15")) && take(guarded.commit()) == 3 && reentrant->guarded &&
            take(memory->snapshot())->rows[0].cells[0].text == "15", "A reentrant editor changed the committing draft");
}
