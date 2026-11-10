#include "LVUINativeSingleSceneFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using View = lvui_generated::LVUINativeSingleSceneFixture;
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
    throw std::runtime_error("Single Scene widget was not found");
}
void single_scene() {
    static_assert(View::scene_count == 1 && View::scenes[0].id == "single" && View::scenes[0].initially_open);
    NativeApplicationBackend::scene_events.clear();
    auto owner = View::make_application_owner();
    good(owner->activate());
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "single_host", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(owner->notify_ready());
    require(NativeApplicationBackend::scene_events == std::vector<std::string>{"opened"},
            "Single Scene did not open on publication");
    auto count = find(tree, "count");
    require(take(count.snapshot()).text == "0", "Single Scene state did not initialize");
    good(find(tree, "increment").activate());
    require(take(count.snapshot()).text == "1", "Single Scene state did not update its widget");
    good(host.clear_content());
    require(!count.snapshot() && NativeApplicationBackend::scene_events == std::vector<std::string>({"opened", "closed"}),
            "Single Scene retirement did not invalidate its widget or deliver onClosed");
    good(owner->retire());
    tree->close();
}
}
int main() { single_scene(); }
