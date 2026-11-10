#include <LVUI/NativeWidgetColors.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
struct Paint {};
struct Drawing { int value; };
struct Painter {
    int value = 0;
    ii::core::Result<void> set_colors(int next) {
        if (next < 0) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid panel");
        value = next; return {};
    }
    ii::core::Result<std::shared_ptr<const Drawing>> operator()(const Paint&) const {
        return std::shared_ptr<const Drawing>(std::make_shared<Drawing>(value));
    }
};
struct TargetState {
    std::function<ii::core::Result<std::shared_ptr<const Drawing>>(const Paint&)> handler;
    unsigned installations = 0, repaints = 0;
};
struct Target {
    std::shared_ptr<TargetState> state;
    ii::core::Result<void> set_paint_handler(decltype(TargetState::handler) handler) const {
        state->handler = std::move(handler); ++state->installations; return {};
    }
    ii::core::Result<void> request_repaint() const { ++state->repaints; return {}; }
};
}
int main() {
    using namespace lvui::native;
    auto target = std::make_shared<TargetState>(); auto graph = std::make_shared<Bindings>();
    ii::core::Observable<int> color{1};
    good(bind_panel_colors<Painter>(*graph, Target{target}, [&](ReadContext& context) { return context.read(color); }));
    require(target->installations == 1 && take(target->handler({}))->value == 1, "Panel binding did not install its painter");
    good(graph->flush()); require(target->repaints == 0, "Unchanged panel requested a repaint");
    color.set(2); require(target->installations == 1 && target->repaints == 1 && take(target->handler({}))->value == 2,
                          "Panel change did not update its existing painter");
    color.set(-1); require(!take(graph->failures()).empty() && take(target->handler({}))->value == 2,
                           "Invalid panel replaced the displayed value");
    color.set(3); require(take(graph->failures()).empty() && take(target->handler({}))->value == 3,
                          "Panel did not recover after an invalid update");
    good(graph->close()); color.set(4);
    require(take(target->handler({}))->value == 3, "Retired panel binding accepted a source update");
}
