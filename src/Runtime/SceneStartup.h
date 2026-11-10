#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

// Owner-thread coordination for the initial attachment of application scenes.
// The caller owns the native windows and decides how to stop the host on failure.
class SceneStartup final {
public:
    using Ready = std::function<ii::core::Result<void>()>;

    explicit SceneStartup(std::size_t scenes, Ready on_ready)
        : SceneStartup(std::vector<bool>(scenes, true), std::move(on_ready)) {}
    explicit SceneStartup(std::vector<bool> required, Ready on_ready)
        : required_(std::move(required)), mounted_(required_.size(), false),
          pending_(static_cast<std::size_t>(std::ranges::count(required_, true))), on_ready_(std::move(on_ready)) {
        if (!pending_ || !on_ready_) throw std::invalid_argument("Scene startup requires initially open scenes and a ready callback");
    }
    SceneStartup(const SceneStartup&) = delete;
    SceneStartup& operator=(const SceneStartup&) = delete;

    [[nodiscard]] ii::core::Result<void> mounted(std::size_t index) {
        if (auto owner = on_owner(); !owner) return owner;
        if (phase_ == Phase::notifying) return ii::core::fail(ii::core::ErrorCode::busy, "Scene readiness is being delivered");
        if (phase_ == Phase::failed) return std::unexpected(*failure_);
        if (phase_ == Phase::cancelled) return ii::core::fail(ii::core::ErrorCode::closed, "Scene startup was cancelled");
        if (index >= mounted_.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Unknown application scene");
        if (!required_[index]) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene is not required for initial readiness");
        if (mounted_[index]) return ii::core::fail(ii::core::ErrorCode::already_exists, "Application scene is already mounted");
        mounted_[index] = true;
        if (--pending_) return {};
        phase_ = Phase::notifying;
        try {
            if (auto ready = on_ready_(); !ready) return record_failure(std::move(ready.error()));
        } catch (...) {
            return record_failure({ii::core::ErrorCode::failed, "Application scene readiness failed", std::current_exception()});
        }
        if (phase_ == Phase::failed) return std::unexpected(*failure_);
        if (phase_ != Phase::cancelled) phase_ = Phase::ready;
        return {};
    }

    [[nodiscard]] ii::core::Result<void> closed(std::size_t index) {
        if (auto owner = on_owner(); !owner) return owner;
        if (index >= mounted_.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Unknown application scene");
        if (phase_ == Phase::failed) return std::unexpected(*failure_);
        if (!required_[index]) return {};
        if (phase_ == Phase::collecting)
            return fail({ii::core::ErrorCode::closed, "Application scene closed before startup completed"});
        return {};
    }

    [[nodiscard]] ii::core::Result<void> fail(ii::core::Error error) {
        if (auto owner = on_owner(); !owner) return owner;
        return record_failure(std::move(error));
    }

    [[nodiscard]] ii::core::Result<void> cancel() {
        if (auto owner = on_owner(); !owner) return owner;
        if (phase_ != Phase::failed) phase_ = Phase::cancelled;
        return {};
    }

    [[nodiscard]] ii::core::Result<void> outcome() const {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        return {};
    }
    template<class Cleanup>
    [[nodiscard]] ii::core::Result<void> finish(Cleanup&& cleanup) const {
        if (auto owner = on_owner(); !owner) return owner;
        try {
            auto result = std::forward<Cleanup>(cleanup)();
            if (failure_) return std::unexpected(*failure_);
            return result;
        } catch (...) {
            if (failure_) return std::unexpected(*failure_);
            return ii::core::fail(ii::core::ErrorCode::failed, "Application scene cleanup failed", std::current_exception());
        }
    }
    [[nodiscard]] bool ready() const noexcept { return phase_ == Phase::ready; }
    [[nodiscard]] bool failed() const noexcept { return phase_ == Phase::failed; }
    [[nodiscard]] bool accepting() const noexcept { return phase_ == Phase::collecting; }
    [[nodiscard]] bool accepts_runtime() const noexcept {
        return phase_ == Phase::collecting || phase_ == Phase::notifying || phase_ == Phase::ready;
    }

private:
    enum class Phase { collecting, notifying, ready, failed, cancelled };
    [[nodiscard]] ii::core::Result<void> record_failure(ii::core::Error error) {
        if (!failure_) failure_ = std::move(error);
        phase_ = Phase::failed;
        return std::unexpected(*failure_);
    }
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Scene startup requires its owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    std::vector<bool> required_, mounted_;
    std::size_t pending_;
    Ready on_ready_;
    Phase phase_ = Phase::collecting;
    std::optional<ii::core::Error> failure_;
};

} // namespace lvui::native
