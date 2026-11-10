#include "ViewSessionCounter.generated.h"
#include "ViewSessionIntegration.generated.h"
#include <LVUI/NativeQueries.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    auto nodes = take(tree->snapshots());
    auto found = std::ranges::find_if(nodes, [&](const auto& node) { return node.id.ends_with(suffix); });
    if (found == nodes.end()) throw std::runtime_error("Session widget missing: " + std::string(suffix));
    return take(tree->find(found->id));
}
std::string displayed(ii::gui::Widget widget) { return take(widget.snapshot()).text; }
void example() {
    using View = lvui_generated::ViewSessionCounter;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {320, 200}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    auto label = find(tree, "/count"), add = find(tree, "/increment"), reset = find(tree, "/reset");
    check(displayed(label) == "0", "Counter initial value was incorrect");
    good(add.activate()); good(add.activate());
    check(displayed(label) == "2", "Button did not update the observed variable");
    auto other_tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto other_host = take(other_tree->content_host({}, "other", {320, 200}));
    std::shared_ptr<View::Scope> other_scope;
    good(other_host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &other_scope); }));
    check(displayed(find(other_tree, "/count")) == "0", "Independent view activation reused another session's state");
    good(reset.activate()); check(displayed(label) == "0", "Reset did not update the label");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset();
    check(weak.expired() && !add.snapshot(), "Retired counter retained its scope or widget");
    good(other_host.clear_content()); other_scope.reset(); other_tree->close(); tree->close();
}
void integration() {
    using View = lvui_generated::ViewSessionIntegration;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {320, 300}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    auto count = find(tree, "/count"), twice = find(tree, "/double");
    auto queried = find(tree, "/query"), object = find(tree, "/object");
    check(displayed(count) == "0" && displayed(twice) == "0" && displayed(queried) == "4",
          "Session values did not initialize");
    check(displayed(object) == "Received object", "C++ object input was not received");
    check(displayed(find(tree, "/recordQuery")) == "10", "A received C++ object's query did not initialize");
    good(find(tree, "/recordCommand").activate());
    check(displayed(find(tree, "/recordQuery")) == "11", "A received C++ object's query command was not observed");
    check(displayed(find(tree, "/localObject")) == "Session object" &&
          displayed(find(tree, "/objectCount")) == "0", "C++ model values were not received by the View");
    good(find(tree, "/numericIncrement").activate());
    check(displayed(find(tree, "/numericObject")) == "1" && scope->backend_0.numericValue.get() == 1,
          "C++ model command did not publish its integer value");
    bool invalid_fraction = false;
    try { auto result = find(tree, "/numericFraction").activate(); invalid_fraction = !result; }
    catch (const std::exception&) { invalid_fraction = true; }
    check(invalid_fraction && displayed(find(tree, "/numericObject")) == "1", "A fractional object assignment lost the integer value");
    good(find(tree, "/objectIncrement").activate());
    check(displayed(find(tree, "/objectCount")) == "1", "C++ model field did not publish");
    good(find(tree, "/renameObject").activate());
    check(displayed(find(tree, "/localObject")) == "Session object updated" &&
          displayed(find(tree, "/objectSummary")) == "Session object updated!" &&
          displayed(find(tree, "/originalObject")) == "Session object",
          "Object component input, derived text or static snapshot did not follow its contract");
    bool invalid_field = false;
    try { auto result = find(tree, "/invalidField").activate(); invalid_field = !result; }
    catch (const std::exception&) { invalid_field = true; }
    check(invalid_field && displayed(find(tree, "/objectCount")) == "1", "Invalid object field replaced the last valid object");
    good(find(tree, "/increment").activate());
    check(displayed(count) == "3" && displayed(twice) == "6", "Sequential assignments read stale derived state");
    good(find(tree, "/localIncrement").activate());
    check(displayed(find(tree, "/localCount")) == "101" && displayed(count) == "3", "A child assignment changed the wrong lexical scope");
    good(find(tree, "/parentIncrement").activate());
    check(displayed(count) == "4" && displayed(find(tree, "/localCount")) == "101", "A child could not update its lexical parent's variable");
    good(find(tree, "/forwarded").activate());
    good(find(tree, "/overridden").activate());
    good(find(tree, "/namedAction").activate());
    good(find(tree, "/variableAction").activate());
    check(displayed(count) == "8", "Forwarded or inline event block lost its lexical owner");
    bool invalid = false;
    try { auto result = find(tree, "/invalidNumber").activate(); invalid = !result; }
    catch (const std::exception&) { invalid = true; }
    check(invalid && displayed(find(tree, "/inferred")) == "4", "Inferred integer accepted a fractional assignment");
    good(find(tree, "/queryCommand").activate());
    check(displayed(queried) == "5", "Query command did not publish its observed result");
    scope->backend_0.selected->title.set("Updated object");
    check(displayed(object) == "Updated object", "C++ object observation did not reach the View");
    scope->backend_0.failing.set(true);
    check(displayed(queried) == "5" && !take(scope->binding_failures()).empty(), "Failed query lost its last valid value or diagnostic");
    scope->backend_0.failing.set(false); scope->backend_0.number.set(7);
    check(displayed(queried) == "7" && take(scope->binding_failures()).empty(), "Query did not recover");
    scope->backend_0.visible.set(false); scope->backend_0.visible.set(true);
    check(displayed(find(tree, "/localCount")) == "100" && displayed(count) == "8", "Reactivated child did not receive independent state");
    auto replacement = std::make_shared<SessionRecord>();
    replacement->number.set(20);
    std::weak_ptr<SessionRecord> previous = scope->backend_0.selected;
    scope->backend_0.selected->onRead = [&] {
        scope->backend_0.selected = replacement;
        scope->revision_0.set(scope->revision_0.get() + 1);
    };
    scope->backend_0.trigger.set(1);
    check(SessionRecord::destroyed_during_query == 0 && displayed(find(tree, "/recordQuery")) == "20",
          "A reentrant query lost its selected C++ owner or failed to follow its replacement");
    check(previous.expired(), "A replaced C++ query receiver remained retained");
    good(host.clear_content()); scope.reset(); tree->close();
}
void query_errors() {
    using ii::runtime::QueryError;
    using ii::runtime::QueryErrorCode;
    ii::runtime::QueryPort<int, int> failing([](const int&) -> int { throw std::runtime_error("handler detail"); });
    try { (void)lvui::native::send_query(failing, 1); throw std::runtime_error("Query failure was accepted"); }
    catch (const lvui::native::QueryFailure<QueryError>& error) {
        check(error.error().code == QueryErrorCode::handler_failed && bool(error.error().cause), "Query handler cause was lost");
    }
    ii::runtime::QueryPort<int, std::expected<int, int>> domain([](const int&) -> std::expected<int, int> {
        return std::unexpected(42);
    });
    const auto response = lvui::native::send_query(domain, 0);
    check(!response && response.error() == 42, "Domain error was confused with transport failure");
    ii::runtime::QueryPort<int, int> closed([](const int&) { return 1; });
    (void)closed.close();
    try { (void)lvui::native::send_query(closed, 0); throw std::runtime_error("Closed query was accepted"); }
    catch (const lvui::native::QueryFailure<QueryError>& error) {
        check(error.error().code == QueryErrorCode::receiver_expired, "Closed query error was lost");
    }
    ii::runtime::Runtime runtime;
    ii::runtime::SerialQueue queue(runtime);
    ii::runtime::QueryPort<int, int> bound(queue, [](const int&) { return 1; });
    try { (void)lvui::native::send_query(bound, 0); throw std::runtime_error("Foreign query executor was accepted"); }
    catch (const lvui::native::QueryFailure<QueryError>& error) {
        check(error.error().code == QueryErrorCode::wrong_executor, "Query executor error was lost");
    }
    auto owner = std::make_shared<std::string>("Retained query text");
    std::weak_ptr<std::string> previous = owner;
    auto result = lvui::native::Methods::with_context([&](lvui::native::ReadContext& context) -> std::string_view {
        context.retain(owner);
        const auto* text = owner.get(); owner.reset();
        return *text;
    });
    static_assert(std::same_as<decltype(result), std::string>);
    check(result == "Retained query text" && previous.expired(), "A retained query returned dangling text");
    bool completed = false;
    lvui::native::Methods::with_context([&](lvui::native::ReadContext&) { completed = true; });
    check(completed, "A void query context did not complete");
}
}
int main() { example(); integration(); query_errors(); }
