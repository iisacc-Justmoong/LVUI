#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <exception>
#include <functional>
#include <thread>
#include <utility>

namespace lvui::native {

// Owns the close policy for exactly one published window generation.
class WindowCloseGuard final {
public:
    using Callback = std::function<bool()>;
    WindowCloseGuard() = default;
    WindowCloseGuard(const WindowCloseGuard&) = delete;
    WindowCloseGuard& operator=(const WindowCloseGuard&) = delete;

    // A View can be mounted again after its preceding generation retired.
    [[nodiscard]] ii::core::Result<void> prepare() {
        if (auto owner = on_owner(); !owner) return owner;
        if (deciding_) return ii::core::fail(ii::core::ErrorCode::busy, "Window close approval is active");
        if (phase_ == Phase::pending && !configured_) return {};
        if (phase_ != Phase::closed)
            return ii::core::fail(ii::core::ErrorCode::busy, "Window close policy is still active");
        phase_ = Phase::pending;
        configured_ = false;
        return {};
    }
    [[nodiscard]] ii::core::Result<void> configure(Callback callback) {
        if (auto owner = on_owner(); !owner) return owner;
        if (phase_ != Phase::pending || configured_)
            return ii::core::fail(ii::core::ErrorCode::busy, "Window close policy is already configured or retired");
        if (!callback) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Window close policy is missing");
        callback_ = std::move(callback);
        configured_ = true;
        return {};
    }
    [[nodiscard]] ii::core::Result<void> publish() {
        if (auto owner = on_owner(); !owner) return owner;
        if (phase_ != Phase::pending || !configured_)
            return ii::core::fail(ii::core::ErrorCode::busy, "Window close policy cannot be published");
        phase_ = Phase::open;
        return {};
    }
    [[nodiscard]] ii::core::Result<bool> request_close() {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        if (phase_ == Phase::pending) return true;
        if (phase_ == Phase::closed || deciding_) return false;
        deciding_ = true;
        struct Reset final { bool& value; ~Reset() { value = false; } } reset{deciding_};
        try {
            auto callback = callback_; // Retirement during the call must not destroy the callable.
            const bool accepted = callback();
            return phase_ == Phase::open && accepted;
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Window close request callback failed", std::current_exception());
        }
    }
    [[nodiscard]] ii::core::Result<void> retire() {
        if (auto owner = on_owner(); !owner) return owner;
        phase_ = Phase::closed;
        callback_ = {};
        return {};
    }

private:
    enum class Phase { pending, open, closed };
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Window close policy requires its owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    Callback callback_;
    Phase phase_ = Phase::pending;
    bool configured_ = false, deciding_ = false;
};

} // namespace lvui::native
