#pragma once

#include <LVUI/NativeBindings.h>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace lvui::native {
// The application supplies the painter and target. The binding graph owns
// change observation; drawing and presentation remain with the host.
template<class Painter, class Target, class Getter>
[[nodiscard]] ii::core::Result<void> bind_panel_colors(Bindings& graph, Target target, Getter getter) {
    using Value = std::remove_cvref_t<std::invoke_result_t<Getter&, ReadContext&>>;
    struct State final { Painter painter; bool installed = false; };
    try {
        auto state = std::make_shared<State>();
        auto id = graph.bind(std::move(getter), [state, target](const Value& value) -> ii::core::Result<void> {
            if (auto updated = state->painter.set_colors(value); !updated) return updated;
            if (!state->installed) {
                auto result = target.set_paint_handler([state](const auto& paint) { return state->painter(paint); });
                if (!result) return result;
                state->installed = true;
                return {};
            }
            return target.request_repaint();
        });
        if (!id) return std::unexpected(id.error());
        return {};
    } catch (...) {
        return ii::core::fail(ii::core::ErrorCode::failed, "Panel binding failed", std::current_exception());
    }
}
} // namespace lvui::native
