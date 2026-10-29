#include "NativeOwnedTabBarSlots.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
                     std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto node = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (node == nodes.end()) throw std::runtime_error("Generated TabBar slot child did not mount");
    return take(tree->find(node->id));
}
}

int main() {
    using View = lvui_generated::NativeOwnedTabBarSlots;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {500, 180}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto first = find(tree, "/first");
    auto second = find(tree, "/second");
    require(take(first.snapshot()).selected && !take(second.snapshot()).selected,
            "A Tab passed through a View sequence was not registered");
    good(second.activate());
    require(take(second.snapshot()).selected && scope->backend_0.selections == std::vector<int>({1}) &&
                scope->backend_0.clicks == 1,
            "A nested Tab did not retain selection and callback routing");
    scope->backend_0.title.set("Updated");
    require(take(second.snapshot()).accessibility_name == "Updated",
            "A nested Tab did not retain its reactive caller input");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    require(weak.expired() && !second.snapshot(), "TabBar slot retained its activation");
    tree->close();
}
