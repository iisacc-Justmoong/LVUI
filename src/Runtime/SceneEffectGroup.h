#pragma once

#include <LVUI/NativeBindings.h>
#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

// Weakly groups one window generation's local and nested binding graphs.
class SceneEffectGroup final {
public:
    static constexpr std::size_t maximum_graphs = 65536;
    SceneEffectGroup() = default;
    SceneEffectGroup(const SceneEffectGroup&) = delete;
    SceneEffectGroup& operator=(const SceneEffectGroup&) = delete;

    [[nodiscard]] ii::core::Result<void> track(const std::shared_ptr<Bindings>& graph) {
        if (auto owner = on_owner(); !owner) return owner;
        if (!graph) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Window binding graph is missing");
        if (retired_) return retire_one(graph);
        if (graphs_.size() == maximum_graphs) {
            std::erase_if(graphs_, [](const auto& weak) { return weak.expired(); });
            if (graphs_.size() == maximum_graphs)
                return ii::core::fail(ii::core::ErrorCode::full, "Window binding group is full");
        }
        try { graphs_.emplace_back(graph); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Cannot track window bindings", std::current_exception()); }
        return {};
    }

    [[nodiscard]] ii::core::Result<void> retire() {
        if (auto owner = on_owner(); !owner) return owner;
        if (std::exchange(retired_, true)) return {};
        auto graphs = std::move(graphs_);
        std::optional<ii::core::Error> first;
        for (const auto& weak : graphs) if (auto graph = weak.lock()) {
            try {
                if (auto result = retire_one(graph); !result && !first) first = result.error();
            } catch (...) {
                if (!first) first = ii::core::Error{ii::core::ErrorCode::failed, "Cannot retire window bindings", std::current_exception()};
            }
        }
        if (first) return std::unexpected(std::move(*first));
        return {};
    }

private:
    [[nodiscard]] static ii::core::Result<void> retire_one(const std::shared_ptr<Bindings>& graph) {
        auto result = graph->retire_effects();
        if (!result && result.error().code == ii::core::ErrorCode::closed) return {};
        return result;
    }
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Window binding group requires its owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    bool retired_ = false;
    std::vector<std::weak_ptr<Bindings>> graphs_;
};

} // namespace lvui::native
