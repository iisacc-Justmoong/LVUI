#include "NativeViewOnly.generated.h"

#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T>
T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

void require_success(ii::core::Result<void> result) {
    if (!result) throw std::runtime_error(result.error().message);
}

ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
                     std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& node) {
        return node.id.ends_with(suffix);
    });
    if (found == nodes.end()) throw std::runtime_error("Counter widget missing");
    return take(tree->find(found->id));
}

} // namespace

int main() {
    using namespace ii::gui;
    using View = lvui_generated::NativeViewOnly;

    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 160}));
    std::shared_ptr<View::Scope> scope;
    require_success(host.set_content([&](WidgetBuilder& builder) {
        return View::build(builder, {}, {}, &scope);
    }));
    auto value = find(tree, "/value");
    auto increment = find(tree, "/increment");
    auto reset = find(tree, "/reset");
    auto* original_model = &scope->backend_0;
    check(scope->backend_0.count.get() == 0 && take(value.snapshot()).text == "0",
          "Import did not create an idle view model");

    (void)take(tree->drawing_snapshot({240, 160}));
    (void)take(tree->drawing_snapshot({240, 160}));
    check(&scope->backend_0 == original_model && scope->backend_0.count.get() == 0,
          "Redraw replaced the view model or invoked its command");
    require_success(increment.activate());
    require_success(increment.activate());
    check(scope->backend_0.count.get() == 2 && take(value.snapshot()).text == "2",
          "C++ command did not publish to the existing view");

    auto other = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto other_host = take(other->content_host({}, "other", {240, 160}));
    std::shared_ptr<View::Scope> other_scope;
    require_success(other_host.set_content([&](WidgetBuilder& builder) {
        return View::build(builder, {}, {}, &other_scope);
    }));
    check(&other_scope->backend_0 != original_model && take(find(other, "/value").snapshot()).text == "0",
          "Separate view activations shared a view model");

    scope->backend_0.count.set(7);
    check(take(value.snapshot()).text == "7", "C++ publication did not update the binding");
    require_success(reset.activate());
    check(take(value.snapshot()).text == "0", "C++ reset command was not delivered");
    scope->backend_0.count.set(std::numeric_limits<std::int64_t>::max());
    require_success(increment.activate());
    check(scope->backend_0.count.get() == std::numeric_limits<std::int64_t>::max(),
          "C++ increment overflowed its value");

    std::weak_ptr<View::Scope> retired = scope;
    require_success(host.clear_content());
    check(!scope->active() && !increment.activate(), "Retired view accepted input");
    scope.reset();
    check(retired.expired(), "Retired view retained its model");
    std::weak_ptr<View::Scope> other_retired = other_scope;
    require_success(other_host.clear_content());
    other_scope.reset();
    check(other_retired.expired(), "Second view model did not retire");
    other->close();
    tree->close();
}
