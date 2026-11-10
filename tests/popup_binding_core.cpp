#include <LVUI/NativePopupOpen.h>
#include <LVUI/NativeStylePort.h>
#include <LVUI/NativeInputEvents.h>

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
struct PopupState { bool opened = false, available = true, blocked = false; int opens = 0, closes = 0; };
struct Popup {
    std::shared_ptr<PopupState> state;
    struct Snapshot { bool opened; };
    ii::core::Result<Snapshot> snapshot() const {
        if (!state->available) return ii::core::fail(ii::core::ErrorCode::closed, "Popup retired");
        return Snapshot{state->opened};
    }
    ii::core::Result<void> open_at(int, int) const {
        if (state->blocked) return ii::core::fail(ii::core::ErrorCode::failed, "Popup blocked");
        state->opened = true; ++state->opens; return {};
    }
    ii::core::Result<void> close_popup() const { state->opened = false; ++state->closes; return {}; }
};
struct Builder {
    std::function<void()> published;
    bool reject = false;
    ii::core::Result<void> on_publish(std::function<void()> callback) {
        if (reject) return ii::core::fail(ii::core::ErrorCode::failed, "Publication rejected");
        published = std::move(callback); return {};
    }
};
enum class LegacyInput { focus_lost, pointer_up };
enum class NewInput { focus_lost, pointer_up, pointer_cancel };
}
int main() {
    using namespace lvui::native;
    auto graph = std::make_shared<Bindings>();
    auto state = std::make_shared<PopupState>();
    ii::core::Observable<bool> desired{true}; Builder builder; unsigned evaluations = 0;
    auto id = take(bind_popup_open(builder, graph, Popup{state}, [&](ReadContext& context) {
        ++evaluations; return context.read(desired);
    }));
    require(!state->opened && state->opens == 0, "Popup opened before publication");
    desired.set(false); desired.set(true);
    require(!state->opened, "Unpublished popup reacted to a source update");
    builder.published();
    require(state->opened && state->opens == 1, "Publication did not apply the latest popup value");
    const auto settled = evaluations;
    good(graph->flush());
    require(evaluations == settled, "Popup binding evaluated without an event");
    desired.set(false); require(!state->opened && state->closes == 1, "Popup did not close on change");
    state->blocked = true; desired.set(true);
    require(!state->opened && !take(graph->failures()).empty(), "Popup failure was not reported");
    state->blocked = false; desired.set(false); desired.set(true);
    require(state->opened && state->opens == 2 && take(graph->failures()).empty(), "Popup did not recover");
    good(graph->remove(id)); desired.set(false);
    require(state->opened && state->closes == 1, "Removed binding kept applying updates");
    Builder rejected; rejected.reject = true; auto failed_graph = std::make_shared<Bindings>();
    auto failed = bind_popup_open(rejected, failed_graph, Popup{state}, [](ReadContext&) { return true; });
    require(!failed && take(failed_graph->stats()).bindings == 0, "Rejected publication retained a binding");
    BorderlessTonePort port{.apply = [](bool) -> ii::core::Result<void> { return {}; }};
    require(pointer_canceled(LegacyInput::focus_lost) && !pointer_canceled(LegacyInput::pointer_up) &&
            pointer_canceled(NewInput::pointer_cancel) && !pointer_canceled(NewInput::pointer_up),
            "Generic cancellation did not preserve legacy and current input semantics");
    good(port.apply(true)); good(graph->close()); good(failed_graph->close());
}
