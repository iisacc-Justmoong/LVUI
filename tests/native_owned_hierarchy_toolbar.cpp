#include <LVUI/NativeHierarchyToolbar.h>
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
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
class Command final : public lvui::HierarchyToolbarCommand {
public:
    std::function<void()> action;
    void invoke(const lvui::ToolbarButtonId&) const override { if (action) action(); }
};
}
int main() {
    using namespace ii::gui;
    const lvui::ToolbarButtonId first_id{std::string("first")}, second_id{std::string("second")}, third_id{std::int64_t(3)};
    auto command = std::make_shared<Command>(); lvui::native::HierarchyToolbarInput input;
    input.items = {{.id=first_id, .icon_name="home"}, {.id=second_id, .icon_name="generaladd", .command=command},
                   {.id=third_id, .icon_name="settings", .enabled=false}};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 80})); std::optional<lvui::native::NativeHierarchyToolbar> toolbar;
    std::vector<std::string> events; bool retire = false, reconfigure = false, confirm_active = false;
    command->action = [&] { events.push_back("command"); };
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading}); if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeHierarchyToolbar::create(builder, *column, "toolbar", input,
            {.active_changed = [&](const auto& selected) { events.push_back("active");
                if (confirm_active) { auto next = input; next.active_id = selected.id; good(toolbar->set_options(next)); }
                if (reconfigure) { auto next = input; next.appearance.width = 250; good(toolbar->set_options(next)); }
                if (retire) good(host.clear_content()); },
             .triggered = [&](const auto&) { events.push_back("triggered"); }});
        if (!value) return std::unexpected(value.error()); toolbar = *value; return {};
    }));
    auto first = take(toolbar->slot(first_id)); auto second = take(toolbar->slot(second_id));
    require(take(toolbar->root().snapshot()).size.width == 200 && take(toolbar->snapshot())->active->id == first_id,
            "Hierarchy toolbar lost its initial size or selection");
    auto initial_id = take(first.snapshot()).id; auto drawing = take(tree->drawing_snapshot({300, 80})); std::size_t images = drawing.images.size();
    for (const auto& layer : drawing.overlays) images += layer.images.size(); require(images == 3, "Hierarchy toolbar did not draw its icons");
    good(second.activate()); require(events == std::vector<std::string>{"active", "triggered", "command"} &&
        take(toolbar->snapshot())->active->id == second_id, "Toolbar activation lost its selection or callback order");
    good(second.activate()); require(events == std::vector<std::string>{"active", "triggered", "command", "triggered", "command"},
        "Activating the current tool repeated a selection event");
    auto reordered = input; std::swap(reordered.items[0], reordered.items[1]); good(toolbar->set_options(reordered));
    require(take(toolbar->snapshot())->active->id == second_id && take(toolbar->slot(first_id).value().snapshot()).id == initial_id,
            "Reordering toolbar items lost selection or rebuilt a surviving keyed button");
    auto invalid = reordered; invalid.items[0].icon_name = "missing-icon";
    require(!toolbar->set_options(invalid) && take(toolbar->snapshot())->active->id == second_id && first.snapshot(),
            "An invalid toolbar icon replaced working items");
    invalid = reordered; invalid.active_id = third_id;
    require(!toolbar->set_options(invalid) && take(toolbar->snapshot())->active->id == second_id,
            "A disabled active toolbar ID replaced the working state");
    auto narrow = reordered; narrow.appearance.width = 16; good(toolbar->set_options(narrow));
    require(!take(first.interaction()).visible && !take(second.interaction()).visible, "Zero-sized toolbar slots remained visible");
    good(toolbar->set_options(reordered));
    require(take(first.interaction()).visible && take(second.interaction()).visible && take(first.snapshot()).id == initial_id,
            "Toolbar items did not recover after narrowing or lost their identities");
    good(first.focus()); good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::home}));
    require(take(toolbar->snapshot())->active->id == second_id, "Toolbar keyboard Home did not preserve the first available item");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::right}));
    require(take(toolbar->snapshot())->active->id == first_id && events.back() == "active", "Toolbar keyboard navigation did not select the next item");
    reordered.items[0].visible = false; good(toolbar->set_options(reordered));
    require(take(toolbar->snapshot())->active->id == first_id && !take(second.interaction()).visible,
            "A hidden toolbar item remained visible or replaced the valid selection");
    reordered.items.erase(reordered.items.begin()); good(toolbar->set_options(reordered));
    require(!second.snapshot() && take(toolbar->slot(first_id).value().snapshot()).id == initial_id,
            "Removing a toolbar item did not preserve its surviving neighbor");
    bool wrong_thread = false;
    std::thread worker([&] { auto result = toolbar->set_options(input); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "Hierarchy toolbar accepted a foreign-thread update");
    good(toolbar->set_options(input)); events.clear(); confirm_active = true; good(take(toolbar->slot(second_id)).activate()); confirm_active = false;
    require(events == std::vector<std::string>{"active", "triggered", "command"},
            "Confirming the selected ID suppressed a valid toolbar action");
    good(toolbar->set_options(input)); good(take(toolbar->slot(first_id)).activate()); events.clear();
    reconfigure = true; good(take(toolbar->slot(second_id)).activate()); reconfigure = false;
    require(events == std::vector<std::string>{"active"}, "A reconfigured toolbar delivered stale follow-up callbacks");
    good(toolbar->set_options(input)); good(take(toolbar->slot(first_id)).activate()); events.clear(); retire = true;
    good(take(toolbar->slot(second_id)).activate());
    require(events == std::vector<std::string>{"active"} && !toolbar->set_options(input), "A retiring toolbar delivered stale actions or accepted updates");
    tree->close();
}
