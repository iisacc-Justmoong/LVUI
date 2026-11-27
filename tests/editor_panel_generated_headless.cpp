#include "EditorPanelFixture.generated.h"

#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}

int main() {
    using View = lvui_generated::EditorPanelFixture;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {640, 420}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(tree->snapshots());
    auto found = std::ranges::find_if(snapshots, [](const auto& node) { return node.text_input; });
    require(found != snapshots.end(), "The editor panel did not mount its text control");
    require(found->accessibility_name == "Draft", "The field did not expose its app-provided label");
    auto field = take(tree->find(found->id));
    good(field.focus());
    good(tree->input({.kind = WidgetInputKind::text, .text = "x"}));
    require(scope->backend_0.events == 1, "The input did not reach the app's viewmodel");
    scope->backend_0.text.set("Updated");
    const auto snapshot = take(field.snapshot());
    require(take(field.text_content({0, snapshot.text_length})).text == "Updated",
            "The viewmodel did not update the existing input");
    require(snapshot.accessibility_name == "Updated", "The accessible label did not follow the viewmodel");
    scope->backend_0.enabled.set(false);
    require(!take(field.snapshot()).enabled, "The field did not track its enabled state");
    good(host.clear_content());
    scope.reset();
    require(!field.snapshot(), "A retired panel retained its input");
    tree->close();
}
