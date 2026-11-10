#include "ImmediateDeclaration.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
}

int main() {
    using View = lvui_generated::ImmediateDeclaration;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 180}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto parent = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/viewArgParent"); });
    const auto child = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/viewArgChild"); });
    const auto sibling = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/viewArgSibling"); });
    require(parent != nodes.end() && child != nodes.end() && sibling != nodes.end(),
            "View argument composition was not mounted");
    require(child->parent_id == parent->id && sibling->parent_id == parent->id && child < sibling,
            "View arguments lost their parent or source order in the native tree");
    auto child_view = take(tree->find(child->id));
    const auto bounds = [&](const char* suffix) {
        const auto found = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(std::string("/") + suffix); });
        require(found != nodes.end(), "Explicit stack fixture was not mounted");
        return found->bounds;
    };
    const auto column = bounds("column"), row = bounds("row");
    require(column.x == row.x && column.y == row.y,
            "ZStack children did not occupy the same origin");
    require(bounds("lower").y > bounds("upper").y && bounds("right").x > bounds("left").x,
            "VStack or HStack did not arrange its children along the selected axis");
    const auto node = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/button"); });
    require(node != nodes.end(), "Immediate button was not mounted");
    auto button = take(tree->find(node->id));
    require(scope->backend_0.clicks == 0, "Event call ran while the view was built");
    require(scope->backend_0.reads > 0, "Value call was not evaluated for the label");
    require(scope->backend_0.factories > 0, "Explicit callback factory was not bound");
    good(button.activate());
    good(button.activate());
    require(scope->backend_0.clicks == 2, "Event call did not run once per activation");
    const auto amountNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/amountButton"); });
    require(amountNode != nodes.end(), "Argument button was not mounted");
    auto amountButton = take(tree->find(amountNode->id));
    const auto factoryNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/factoryButton"); });
    require(factoryNode != nodes.end(), "Factory button was not mounted");
    auto factoryButton = take(tree->find(factoryNode->id));
    const auto factories = scope->backend_0.factories;
    scope->backend_0.increment.set(3);
    require(scope->backend_0.factories > factories, "Callback factory did not follow its dependency");
    good(amountButton.activate());
    require(scope->backend_0.clicks == 5, "Event arguments were captured before activation");
    good(factoryButton.activate());
    require(scope->backend_0.clicks == 8, "Callback factory did not deliver its latest returned function");
    const auto nestedNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/nested"); });
    require(nestedNode != nodes.end(), "Reusable view did not mount its action");
    auto nested = take(tree->find(nestedNode->id));
    good(nested.activate());
    require(scope->backend_0.clicks == 9, "Reusable view lost its event call's original owner");
    const auto orderNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/orderButton"); });
    require(orderNode != nodes.end() && scope->backend_0.evaluation_order.empty(), "Expression ran before its event");
    good(take(tree->find(orderNode->id)).activate());
    require(scope->backend_0.evaluation_order == std::vector<int>({1, 2, 3, 4, 5, 6}) &&
            scope->backend_0.recorded_pair == std::pair<int, int>({1, 2}) && scope->backend_0.computed == 34 && !scope->backend_0.compared,
            "Script arguments and operands did not evaluate once in source order");
    const auto queryNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/queryButton"); });
    require(queryNode != nodes.end() && scope->backend_0.query_calls == 0, "A query ran before its event");
    good(take(tree->find(queryNode->id)).activate());
    require(scope->backend_0.evaluation_order == std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9}) &&
            scope->backend_0.query_calls == 1 && scope->backend_0.last_query == 17,
            "A script query did not evaluate its port and request once in order");
    const auto typedNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/typedOrderButton"); });
    require(typedNode != nodes.end(), "Typed callable button did not mount");
    good(take(tree->find(typedNode->id)).activate());
    require(scope->backend_0.evaluation_order == std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}) &&
            scope->backend_0.recorded_pair == std::pair<int, int>({10, 11}) && !scope->backend_0.compared && scope->backend_0.chosen == 1,
            "Typed C++ callables did not preserve script argument order");
    const auto flowNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/flowButton"); });
    require(flowNode != nodes.end() && scope->backend_0.item_queries == 0 && scope->backend_0.recorded.empty(),
            "A script condition or loop ran without an event");
    auto flowButton = take(tree->find(flowNode->id));
    good(flowButton.activate());
    require(scope->backend_0.item_queries == 1 && scope->backend_0.recorded == std::vector<int>({1, 3, 42}),
            "A loop did not snapshot once, continue, break, and preserve its source order");
    const auto nestedFlowNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/nestedFlowButton"); });
    require(nestedFlowNode != nodes.end(), "Nested flow button did not mount");
    good(take(tree->find(nestedFlowNode->id)).activate());
    require(scope->backend_0.recorded == std::vector<int>({1, 3, 42, 12, 1, 22, 2}),
            "Nested loop bindings, continue, or break escaped their nearest loop");
    const auto scriptNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/scriptButton"); });
    require(scriptNode != nodes.end() && scope->backend_0.opened == 0, "Script ran without an event");
    auto scriptButton = take(tree->find(scriptNode->id));
    good(scriptButton.activate());
    require(scope->backend_0.opened == 1 && scope->backend_0.last_open,
            "Tagless script did not execute assignments and calls in order");
    good(flowButton.activate());
    require(scope->backend_0.item_queries == 1 && scope->backend_0.recorded == std::vector<int>({1, 3, 42, 12, 1, 22, 2, 7, 9}),
            "A script condition or nested return did not stop at the correct scope");
    const auto retireNode = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/retireButton"); });
    require(retireNode != nodes.end(), "Script retirement button did not mount");
    auto retireButton = take(tree->find(retireNode->id));
    scope->backend_0.retire_action = [&] { good(host.clear_content()); };
    good(retireButton.activate());
    require(scope->backend_0.after_retire == 0 && !button.snapshot() && !child_view.snapshot(),
            "Script continued after its view was retired");
    std::weak_ptr<View::Scope> lifetime = scope;
    scope.reset();
    require(lifetime.expired() && !button.snapshot(), "A retired view retained its event target");
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto remounted = take(tree->snapshots());
    const auto conditionNode = std::ranges::find_if(remounted, [](const auto& value) { return value.id.ends_with("/conditionRetireButton"); });
    require(conditionNode != remounted.end(), "Condition retirement button did not mount");
    auto conditionButton = take(tree->find(conditionNode->id));
    scope->backend_0.retire_action = [&] { good(host.clear_content()); };
    good(conditionButton.activate());
    require(scope->backend_0.after_retire == 0, "A condition ran its branch after retiring the view");
    scope.reset();
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto copyNodes = take(tree->snapshots());
    const auto copyNode = std::ranges::find_if(copyNodes, [](const auto& value) { return value.id.ends_with("/flowButton"); });
    require(copyNode != copyNodes.end(), "Snapshot retirement button did not mount");
    auto copyButton = take(tree->find(copyNode->id));
    int copies = 0;
    scope->backend_0.on_item_copy = [&] { ++copies; good(host.clear_content()); };
    for (auto& item : scope->backend_0.items) item.on_copy = &scope->backend_0.on_item_copy;
    good(copyButton.activate());
    require(copies == 1 && scope->backend_0.item_queries == 1 && scope->backend_0.recorded.empty(),
            "A retired view continued copying or executing its loop snapshot");
    scope.reset();
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto sourceNodes = take(tree->snapshots());
    const auto sourceNode = std::ranges::find_if(sourceNodes, [](const auto& value) { return value.id.ends_with("/sourceRetireButton"); });
    require(sourceNode != sourceNodes.end(), "Source retirement button did not mount");
    auto sourceButton = take(tree->find(sourceNode->id));
    copies = 0;
    scope->backend_0.on_item_copy = [&] { ++copies; };
    for (auto& item : scope->backend_0.items) item.on_copy = &scope->backend_0.on_item_copy;
    scope->backend_0.retire_action = [&] { good(host.clear_content()); };
    good(sourceButton.activate());
    require(copies == 0 && scope->backend_0.after_retire == 0,
            "A retired view copied or executed a loop after evaluating its source");
    scope.reset();
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto argumentNodes = take(tree->snapshots());
    const auto argumentNode = std::ranges::find_if(argumentNodes, [](const auto& value) { return value.id.ends_with("/argumentRetireButton"); });
    require(argumentNode != argumentNodes.end(), "Argument retirement button did not mount");
    auto argumentButton = take(tree->find(argumentNode->id));
    scope->backend_0.retire_action = [&] { good(host.clear_content()); };
    good(argumentButton.activate());
    require(scope->backend_0.after_retire == 0 && scope->backend_0.recorded_pair == std::pair<int, int>{},
            "A retired view evaluated another argument or invoked the outer method");
    scope.reset();
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto queryRetireNodes = take(tree->snapshots());
    const auto queryRetireNode = std::ranges::find_if(queryRetireNodes, [](const auto& value) { return value.id.ends_with("/queryRetireButton"); });
    require(queryRetireNode != queryRetireNodes.end(), "Query retirement button did not mount");
    auto queryRetireButton = take(tree->find(queryRetireNode->id));
    scope->backend_0.retire_action = [&] { good(host.clear_content()); };
    good(queryRetireButton.activate());
    require(scope->backend_0.after_retire == 0 && scope->backend_0.query_calls == 0,
            "A retired view evaluated the rest of a query request or sent it");
    scope.reset();
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto failureNodes = take(tree->snapshots());
    const auto failureNode = std::ranges::find_if(failureNodes, [](const auto& value) { return value.id.ends_with("/flowButton"); });
    require(failureNode != failureNodes.end(), "Snapshot failure button did not mount");
    auto failureButton = take(tree->find(failureNode->id));
    scope->backend_0.on_item_copy = [] { throw std::runtime_error("snapshot copy failed"); };
    for (auto& item : scope->backend_0.items) item.on_copy = &scope->backend_0.on_item_copy;
    bool copyFailed = false;
    try { good(failureButton.activate()); }
    catch (const std::runtime_error& error) { copyFailed = std::string(error.what()) == "snapshot copy failed"; }
    require(copyFailed && scope->active() && scope->backend_0.item_queries == 1 && scope->backend_0.recorded.empty(),
            "A snapshot failure was hidden or partially executed the loop");
    scope->backend_0.on_item_copy = {};
    good(failureButton.activate());
    require(scope->backend_0.item_queries == 2 && scope->backend_0.recorded == std::vector<int>({1, 3, 42}),
            "A failed snapshot did not recover on the next requested event");
    std::weak_ptr<View::Scope> recoveredLifetime = scope;
    good(host.clear_content());
    scope.reset();
    require(recoveredLifetime.expired(), "A recovered script retained its retired view");
    tree->close();
}
