#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyItemProjection.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
std::vector<std::string> keys(const std::vector<lvui::HierarchyEntry>& entries) {
    std::vector<std::string> result; for (const auto& entry : entries) result.push_back(entry.key); return result;
}
std::vector<std::string> visible(const lvui::HierarchyProjection& projection) {
    std::vector<std::string> result; for (const auto& row : projection.entries) result.push_back(row.entry.key); return result;
}
class ReadOnly final : public lvui::HierarchySource {
public:
    explicit ReadOnly(std::shared_ptr<const lvui::HierarchySnapshot> input) : value(std::move(input)) {}
    lvui::HierarchyResult<std::shared_ptr<const lvui::HierarchySnapshot>> snapshot() const override { return value; }
    std::shared_ptr<const lvui::HierarchySnapshot> value;
};
}

int main() {
    using namespace lvui;
    const std::vector<HierarchyEntry> initial{
        {.key="a", .label="A"}, {.key="a1", .parent="a", .label="A1"},
        {.key="a1x", .parent="a1", .label="A1 child"}, {.key="a2", .parent="a", .label="A2"},
        {.key="b", .label="B"}, {.key="b1", .parent="b", .label="B1"}, {.key="c", .label="C"}};
    auto model = std::make_shared<HierarchyMemory>(initial);
    auto original = take(model->snapshot()); auto index = take(HierarchyIndex::create(original));
    require(index.items().size() == 7 && index.items()[0].end == 4 && index.items()[0].children == 2 &&
            index.items()[2].depth == 2 && index.items()[4].end == 6, "Hierarchy subtree index is incorrect");
    auto source = std::make_shared<ReadOnly>(original); HierarchyController view(source);
    require(visible(take(view.projection())) == std::vector<std::string>{"a", "b", "c"} && take(view.source_snapshot()) == original,
            "Collapsed projection or source snapshot is incorrect");
    require(take(view.set_expanded("a", true)) && take(view.select("a1x")) &&
            visible(take(view.projection())) == std::vector<std::string>{"a", "a1", "a2", "b", "c"},
            "Expansion or hidden selection was not kept as view state");
    require(take(view.set_expanded("a1", true)) && take(view.projection()).entries[2].selected,
            "Expanding a branch lost the selected descendant");
    auto style = HierarchyItemConfiguration{}; style.width = 240;
    const auto visual = hierarchy_item_configuration(take(view.projection()).entries[2], style);
    require(visual && visual->key == "a1x" && visual->depth == 2 && visual->selected && visual->width == 240 && !visual->has_children,
            "Hierarchy row projection lost source state or caller appearance");
    require(take(view.set_expanded("a", false)) && take(view.selected_key()) == "a1x" &&
            original->revision == 0 && take(model->snapshot())->revision == 0,
            "Navigation changed the document or discarded hidden selection");
    require(!view.set_expanded("a2", true) && !view.select("missing"), "Invalid view navigation was accepted");
    bool foreign = false;
    std::thread worker([&] { auto result = view.projection(); foreign = !result && result.error().code == HierarchyErrorCode::wrong_thread; });
    worker.join(); require(foreign, "Hierarchy view accepted a foreign-thread request");

    const auto no_move = take(index.plan({"a2", "a1", HierarchyPlacement::after, 0}));
    require(no_move.unchanged && take(model->move({"a2", "a1", HierarchyPlacement::after, 0})) == 0,
            "An unchanged move published a document revision");
    require(!index.plan({"a", "a1x", HierarchyPlacement::child, 0}) &&
            !index.plan({"a", "a", HierarchyPlacement::before, 0}), "A subtree could move into itself");
    const auto revision = take(model->move({"a1", "b", HierarchyPlacement::after, 0}));
    auto moved = take(model->snapshot());
    require(revision == 1 && keys(moved->entries) == std::vector<std::string>{"a", "a2", "b", "b1", "a1", "a1x", "c"} &&
            moved->entries[4].parent.empty() && moved->entries[5].parent == "a1" && original->entries == initial,
            "Moving a subtree changed its descendants or an earlier snapshot");
    require(!model->move({"a2", "b", HierarchyPlacement::child, 0}) && take(model->snapshot())->revision == 1,
            "A stale hierarchy move changed the model");
    require(take(model->move({"b", "a1", HierarchyPlacement::child, 1})) == 2,
            "Moving a subtree into another parent failed");
    auto nested = take(model->snapshot());
    require(keys(nested->entries) == std::vector<std::string>{"a", "a2", "a1", "a1x", "b", "b1", "c"} &&
            nested->entries[4].parent == "a1" && nested->entries[5].parent == "b",
            "Nested move lost preorder or child parentage");
    auto invalid = initial; invalid.push_back({.key="late", .parent="a", .label="Out of order"});
    require(!model->replace(invalid, 2) && take(model->snapshot())->revision == 2,
            "An invalid replacement changed the committed hierarchy");
    invalid = initial; invalid[1].key = "a";
    require(!HierarchyIndex::create(std::make_shared<HierarchySnapshot>(HierarchySnapshot{invalid, 0})),
            "Duplicate hierarchy keys were accepted");
    auto expected_keys = keys(initial); std::ranges::sort(expected_keys);
    for (const auto& source_entry : initial) for (const auto& target_entry : initial)
        for (const auto placement : {HierarchyPlacement::before, HierarchyPlacement::after, HierarchyPlacement::child}) {
            HierarchyMemory trial(initial);
            auto outcome = trial.move({source_entry.key, target_entry.key, placement, 0});
            if (!outcome) continue;
            auto result = take(trial.snapshot()); auto check = HierarchyIndex::create(result);
            auto result_keys = keys(result->entries); std::ranges::sort(result_keys);
            require(check && result_keys == expected_keys && result->revision == *outcome,
                    "A supported move lost a node or broke preorder");
            for (const auto& entry : result->entries) if (entry.key != source_entry.key) {
                const auto before = std::ranges::find_if(initial, [&](const auto& item) { return item.key == entry.key; });
                require(before != initial.end() && *before == entry, "A subtree move changed an unrelated entry");
            }
        }
    HierarchyMemory root_move(initial);
    require(take(root_move.move({"a1", "", HierarchyPlacement::root_end, 0})) == 1 &&
            keys(take(root_move.snapshot())->entries) == std::vector<std::string>{"a", "a2", "b", "b1", "c", "a1", "a1x"},
            "Root-end move did not preserve the subtree");
    HierarchyMemory concurrent(initial); auto left = initial, right = initial;
    left[0].label = "Left"; right[0].label = "Right"; std::atomic<int> winners{0};
    std::thread first([&] { if (concurrent.replace(left, 0)) ++winners; });
    std::thread second([&] { if (concurrent.replace(right, 0)) ++winners; });
    first.join(); second.join();
    require(winners == 1 && take(concurrent.snapshot())->revision == 1,
            "Concurrent hierarchy replacements both committed the same revision");
    std::vector<HierarchyEntry> too_many(hierarchy_maximum_items + 1);
    require(!HierarchyIndex::create(std::make_shared<HierarchySnapshot>(HierarchySnapshot{std::move(too_many), 0})),
            "Hierarchy accepted more than its bounded item count");
    require(!model->replace(initial, 0), "A stale hierarchy replacement was accepted");
    source->value = nested;
    require(take(view.set_expanded("b", true)), "A moved branch could not be expanded");
    require(visible(take(view.projection())) == std::vector<std::string>{"a", "a1", "a1x", "b", "b1", "c"} &&
            take(view.selected_key()) == "a1x", "A new model projection did not retain surviving view state");
    auto shortened = std::make_shared<HierarchySnapshot>(HierarchySnapshot{{{.key="c", .label="C"}}, 3});
    source->value = shortened; auto latest = take(view.projection());
    require(visible(latest) == std::vector<std::string>{"c"} && take(view.selected_key()).empty(),
            "Removed hierarchy selection was not retired from view state");
}
