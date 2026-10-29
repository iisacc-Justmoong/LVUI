#include "NativeOwnedCodeEditor.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}
int main() {
    using View = lvui_generated::NativeOwnedCodeEditor;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 120})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto nodes = take(tree->snapshots()); auto found = std::ranges::find_if(nodes, [](const auto& value) { return value.accessibility_name == "Independent code editor"; });
    require(found != nodes.end(), "Generated code editor did not mount"); auto widget = take(tree->find(found->id));
    require(take(widget.text_content({0,5})).text == "alpha", "Generated editor lost its document source");
    good(widget.focus()); good(tree->input({.kind=ii::gui::WidgetInputKind::text,.text="x"}));
    auto graph=scope->native_lifetime.lock();
    require(scope->backend_0.edits == 1 && scope->backend_0.selections == 1 && take(graph->failures()).empty() &&
            take(widget.text_content({0,6})).text == "xalpha" && take(widget.snapshot()).size.width == 230,
            "Generated editor lost callbacks or rejected reactive updates during an edit");
    scope->backend_0.width.set(200); require(take(widget.snapshot()).size.width == 200, "Generated editor did not apply reactive appearance");
    (void)take(scope->backend_0.document->replace({.revision=1,.range={0,0},.text="!"})); scope->backend_0.revision.set(2);
    require(take(widget.text_content({0,7})).text == "!xalpha" && take(graph->failures()).empty(),
            "Generated editor did not refresh after an external edit");
    scope->backend_0.external_during_edit = true;
    good(tree->input({.kind=ii::gui::WidgetInputKind::text,.text="Y"}));
    require(take(scope->backend_0.document->snapshot())->revision() == 4 &&
            take(widget.text_content({0,9})).text == "?!Yxalpha" && take(graph->failures()).empty(),
            "Generated editor lost a source update made during an edit callback");
    std::weak_ptr<View::Scope> weak=scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !widget.snapshot(), "Generated code editor retained its activation"); tree->close();
}
