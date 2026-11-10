#include "NativeDefaultBinding.generated.h"

#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

void good(ii::core::Result<void> result) {
    if (!result) throw std::runtime_error(result.error().message);
}

ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
                     std::string_view suffix) {
    auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& node) {
        return node.id.ends_with(suffix);
    });
    if (found == nodes.end()) throw std::runtime_error("Binding test widget missing: " + std::string(suffix));
    return take(tree->find(found->id));
}

void type(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,
          ii::gui::Widget field, const std::string& text) {
    good(field.focus());
    good(tree->input({.kind = ii::gui::WidgetInputKind::text, .text = text}));
}

} // namespace

int main() {
    using namespace ii::gui;
    using View = lvui_generated::NativeDefaultBinding;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {600, 1200}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    (void)take(tree->drawing_snapshot({600, 1200}));

    type(tree, find(tree, "/local"), "!");
    check(scope->value_draft.get() == "Local!" && take(find(tree, "/localLabel").snapshot()).text == "Local!",
          "Local state did not bind in both directions");
    check(scope->derived_draftCopy->get() == "Local!" &&
          take(find(tree, "/localDerivedLabel").snapshot()).text == "Local!",
          "A const did not observe the local var it depends on");
    type(tree, find(tree, "/explicit"), "!");
    check(scope->value_explicitValue.get() == "Explicit!" && take(find(tree, "/explicitLabel").snapshot()).text == "Explicit!",
          "Explicit default markers changed binding behavior");
    type(tree, find(tree, "/viaLocal"), "+");
    check(scope->value_draft.get() == "Local!+", "Reusable view lost its local source");
    type(tree, find(tree, "/model"), "!");
    check(scope->backend_0.text.get() == "Draft!" &&
          take(find(tree, "/modelLabel").snapshot()).text == "Draft!",
          "C++ source lost automatic binding");
    type(tree, find(tree, "/viaModel"), "+");
    check(scope->backend_0.text.get() == "Draft!+" && take(find(tree, "/modelLabel").snapshot()).text == "Draft!+",
          "Reusable view lost its C++ source");
    check(scope->state_scratch->get() == "Draft" && scope->inferred_inferred->get() == "Draft",
          "Local state was overwritten by a later C++ change");
    type(tree, find(tree, "/scratch"), "!");
    type(tree, find(tree, "/inferred"), "+");
    check(scope->state_scratch->get() == "Draft!" && scope->inferred_inferred->get() == "Draft+" &&
          scope->backend_0.text.get() == "Draft!+", "A local initialized state wrote to its initializer");
    type(tree, find(tree, "/command"), "!");
    check(scope->backend_0.guarded.get() == "Guarded" && scope->backend_0.edits == std::vector<std::string>{"Guarded!"},
          "Automatic binding bypassed an explicit edit command");
    type(tree, find(tree, "/editor"), "!");
    type(tree, find(tree, "/search.editor"), "!");
    check(scope->backend_0.editor.get() == "Editor!" && scope->backend_0.query.get() == "Find!",
          "Editor or search did not publish its text");

    good(find(tree, "/localCheck").activate());
    check(scope->value_selected.get() && take(find(tree, "/localCheckLabel").snapshot()).text == "On",
          "CheckBox did not publish local state");
    good(find(tree, "/modelCheck").activate());
    check(scope->backend_0.checked.get(), "CheckBox did not publish C++ state");
    good(find(tree, "/commandCheck").activate());
    check(!scope->backend_0.guardedCheck.get() && scope->backend_0.toggles == std::vector<bool>{true},
          "Automatic binding bypassed an explicit toggle command");
    good(find(tree, "/toggle").activate());
    good(find(tree, "/radio").activate());
    check(scope->backend_0.enabled.get() && scope->backend_0.radio.get(),
          "Toggle or radio did not publish C++ state");
    auto slider = find(tree, "/slider");
    good(slider.focus());
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::right}));
    check(scope->backend_0.level.get() > 0.2, "Slider did not publish its value");
    good(find(tree, "/secondTab").activate());
    check(scope->backend_0.tab.get() == 1, "TabBar did not publish its selection");
    auto selector = find(tree, "/control");
    good(selector.focus());
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::down}));
    check(scope->backend_0.choice.get() == 1, "List selector did not publish its selection");

    type(tree, find(tree, "/derived"), "x");
    type(tree, find(tree, "/fixed"), "x");
    type(tree, find(tree, "/readonly"), "x");
    check(scope->backend_0.text.get() == "Draft!+" && scope->backend_0.locked.get() == "Locked" &&
          *scope->fixed_fixed == "Draft", "Read-only or static input wrote to its source");
    scope->backend_0.text.set("External");
    check(take(find(tree, "/modelLabel").snapshot()).text == "External" && *scope->fixed_fixed == "Draft",
          "Reactive update or static snapshot was lost");
    check(scope->derived_derived->get() == "External" &&
          take(find(tree, "/derivedLabel").snapshot()).text == "External",
          "A const derived from C++ stopped observing its source");
    (void)take(tree->drawing_snapshot({600, 1200}));
    check(scope->value_draft.get() == "Local!+" && scope->state_scratch->get() == "Draft!",
          "Redrawing recreated activation state");

    auto other = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto other_host = take(other->content_host({}, "other", {600, 1200}));
    std::shared_ptr<View::Scope> other_scope;
    good(other_host.set_content([&](WidgetBuilder& builder) { return View::build(builder, {}, {}, &other_scope); }));
    check(other_scope->value_draft.get() == "Local" && !other_scope->value_selected.get() &&
          other_scope->backend_0.text.get() == "Draft", "Independent activation shared state");
    auto retired_input = find(tree, "/local");
    std::weak_ptr<View::Scope> retired = scope;
    good(host.clear_content());
    scope.reset();
    check(retired.expired() && !retired_input.focus(), "Retired bindings retained the view");
    good(other_host.clear_content());
    other_scope.reset();
    other->close();
    tree->close();
}
