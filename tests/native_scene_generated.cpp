#include "LVUINativeScenesFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using View = lvui_generated::LVUINativeScenesFixture;
using State = ii::gui::detail::WidgetState;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
ii::gui::Widget find(const std::shared_ptr<State>& tree, std::string_view name) {
    for (const auto& node : take(tree->snapshots()))
        if (node.id.ends_with("/" + std::string(name))) return take(tree->find(node.id));
    throw std::runtime_error("Scene widget was not found: " + std::string(name));
}
bool scene_has_text(const ii::gui::CanvasScene& scene, std::string_view value) {
    for (const auto& layer : scene.layers)
        for (const auto& text : layer.texts)
            if (text.text == value) return true;
    return false;
}
struct Generation final {
    std::shared_ptr<State> tree;
    ii::gui::Widget host;
    bool closed = false;
};
void scene_generation_and_render_tree() {
    static_assert(View::scene_count == 3 && View::scenes.size() == 3);
    static_assert(View::scenes[0].id == "main" && View::scenes[0].initially_open);
    static_assert(View::scenes[1].id == "tools" && !View::scenes[1].initially_open);
    static_assert(View::scenes[2].id == "legacy" && View::scenes[2].initially_open);
    require(take(View::options_for_scene(1)).title == "Tools", "Scene window options were lost");

    const auto previous_created = NativeWindowBackend::created;
    const auto previous_destroyed = NativeWindowBackend::destroyed;
    NativeApplicationBackend::scene_events.clear();
    NativeWindowBackend::scene_events.clear();
    ii::runtime::EventLoop loop;
    int errors = 0, generations = 0;
    auto owner = View::make_application_owner();
    auto registry = std::make_shared<lvui::native::SceneRegistry>(
        [sender = loop.sender()](auto task) -> ii::core::Result<void> {
            if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Event loop rejected request");
            return {};
        }, [&](const auto&) { ++errors; });
    std::weak_ptr<Generation> latest;
    good(registry->define("tools", [&](bool initial) -> ii::core::Result<lvui::native::SceneInstance> {
        require(!initial, "Optional scene opened during startup");
        auto generation = std::make_shared<Generation>();
        generation->tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
        generation->host = take(generation->tree->content_host({}, "tools_host", {480, 360}));
        good(generation->host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 1); }));
        latest = generation;
        ++generations;
        return lvui::native::SceneInstance{generation,
            [generation]() -> ii::core::Result<bool> { return generation->closed; },
            [generation]() -> ii::core::Result<void> {
                if (auto cleared = generation->host.clear_content(); !cleared) return cleared;
                generation->closed = true;
                generation->tree->close();
                return {};
            }};
    }));
    good(owner->bind_scene_registry(registry));
    good(owner->activate());
    auto main = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(main->content_host({}, "main_host", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    good(owner->notify_ready());
    require(NativeApplicationBackend::scene_events == std::vector<std::string>{"opened"},
            "Scene publication did not deliver onOpened");
    auto count = find(main, "count");
    require(take(count.snapshot()).text == "0", "Initial Scene state is missing");
    (void)take(main->drawing_snapshot({640, 400}));
    auto content = find(main, "sceneContent");
    const auto before = take(content.snapshot_scene_graph());
    require(scene_has_text(before.scene, "0"), "Scene content did not enter iiGUI's render tree");
    good(find(main, "increment").activate());
    require(take(count.snapshot()).text == "1", "Scene event did not update its view session");
    (void)take(main->drawing_snapshot({640, 400}));
    const auto after = take(content.snapshot_scene_graph());
    require(scene_has_text(after.scene, "1") && scene_has_text(before.scene, "0"),
            "Render graph did not update or its earlier snapshot changed");

    good(find(main, "open").activate());
    require(generations == 0 && take(count.snapshot()).text == "2",
            "Scene opening ran inside the event callback or lost its earlier state update");
    loop.poll();
    auto first = latest.lock();
    require(first && generations == 1 && NativeWindowBackend::created == previous_created + 1 &&
            take(find(first->tree, "local").snapshot()).text == "10", "Scene did not activate its own state");
    good(find(first->tree, "localIncrement").activate());
    require(take(find(first->tree, "local").snapshot()).text == "11", "Scene-local update failed");
    auto retired_button = find(first->tree, "closeSelf");
    good(retired_button.activate());
    loop.poll();
    require(first->closed && !retired_button.snapshot() && NativeWindowBackend::destroyed == previous_destroyed + 1,
            "Closing Scene did not retire its tree and backend");
    good(find(main, "open").activate());
    loop.poll();
    auto second = latest.lock();
    require(second && second != first && generations == 2 &&
            take(find(second->tree, "local").snapshot()).text == "10" && take(count.snapshot()).text == "3",
            "Reopening did not reset only the closed Scene");
    good(find(main, "close").activate());
    loop.poll();
    require(second->closed && NativeWindowBackend::destroyed == previous_destroyed + 2 && errors == 0,
            "Scene close command or generation retirement failed");
    good(find(main, "openNamed").activate());
    loop.poll();
    auto third = latest.lock();
    require(third && third != second && generations == 3 && take(count.snapshot()).text == "3",
            "Application-scoped callback did not open the intended Scene");
    good(find(main, "close").activate());
    loop.poll();
    require(third->closed && NativeWindowBackend::destroyed == previous_destroyed + 3,
            "Application callback's Scene did not retire");
    good(find(main, "openDocument").activate());
    loop.poll();
    auto fourth = latest.lock();
    require(fourth && fourth != third && generations == 4,
            "Document-scoped callback did not open the intended Scene");
    good(registry->shutdown());
    require(fourth->closed && NativeWindowBackend::destroyed == previous_destroyed + 4 &&
            NativeWindowBackend::scene_events.size() == 8, "Scene lifecycle events were lost or repeated");
    good(host.clear_content());
    require(NativeApplicationBackend::scene_events == std::vector<std::string>({"opened", "closed"}),
            "Main Scene did not receive exactly one closed event");
    good(owner->retire());
    main->close();
}
}
int main() { scene_generation_and_render_tree(); }
