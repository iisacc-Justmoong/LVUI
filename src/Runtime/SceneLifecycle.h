#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <exception>
#include <functional>
#include <thread>
#include <utility>

namespace lvui::native {

// Coordinates a single published window generation on its owner thread.
class SceneLifecycle final {
public:
    using Callback = std::function<void()>;
    SceneLifecycle() = default;
    SceneLifecycle(const SceneLifecycle&) = delete;
    SceneLifecycle& operator=(const SceneLifecycle&) = delete;

    [[nodiscard]] ii::core::Result<void> configure(Callback opened, Callback closed) {
        if (auto owner = on_owner(); !owner) return owner;
        if (configured_ || phase_ != Phase::pending)
            return ii::core::fail(ii::core::ErrorCode::busy, "Window lifecycle is already configured or retired");
        opened_ = std::move(opened);
        closed_ = std::move(closed);
        configured_ = true;
        return {};
    }

    [[nodiscard]] ii::core::Result<void> opened() {
        if (auto owner = on_owner(); !owner) return owner;
        if (phase_ != Phase::pending) return {};
        if (!configured_) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Window lifecycle is not configured");
        phase_ = Phase::open;
        try { auto callback = opened_; if (callback) callback(); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Window opened callback failed", std::current_exception()); }
        return {};
    }

    [[nodiscard]] ii::core::Result<void> closed() {
        if (auto owner = on_owner(); !owner) return owner;
        const auto previous = std::exchange(phase_, Phase::closed);
        if (previous != Phase::open) return {};
        try { auto callback = closed_; if (callback) callback(); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Window closed callback failed", std::current_exception()); }
        return {};
    }

private:
    enum class Phase { pending, open, closed };
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Window lifecycle requires its owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    Phase phase_ = Phase::pending;
    bool configured_ = false;
    Callback opened_, closed_;
};

} // namespace lvui::native
