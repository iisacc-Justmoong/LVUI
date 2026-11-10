#pragma once

#include "WindowCloseGuard.h"
#include <iiSDKCore/Core/Result/Result.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

// Tracks independently closing native window generations on the UI owner.
// The host adapter owns the actual windows and handles app termination.
class SceneWindowSet final {
public:
    struct Ticket final {
        std::size_t index = 0;
        std::uint64_t generation = 0;
        friend bool operator==(const Ticket&, const Ticket&) = default;
    };
    using LastClose = std::function<ii::core::Result<bool>()>;
    static constexpr std::size_t maximum_scenes = 4096;

    SceneWindowSet(std::size_t count, LastClose last_close)
        : slots_(valid_count(count)), last_close_(std::move(last_close)) {
        if (!last_close_)
            throw std::invalid_argument("Application scenes require valid slots and a last-window decision");
    }
    SceneWindowSet(const SceneWindowSet&) = delete;
    SceneWindowSet& operator=(const SceneWindowSet&) = delete;

    // The adapter may replace an active or closing generation only after
    // independently confirming that its native window has closed.
    [[nodiscard]] ii::core::Result<Ticket> activate(std::size_t index, bool previous_native_closed = false,
                                                    std::shared_ptr<WindowCloseGuard> guard = {}) {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        if (!accepting_) return ii::core::fail(ii::core::ErrorCode::closed, "Application scenes are closed");
        if (deciding_) return ii::core::fail(ii::core::ErrorCode::busy, "Application close approval is active");
        if (index >= slots_.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Unknown application scene");
        auto& slot = slots_[index];
        if (slot.phase != Phase::closed && !previous_native_closed)
            return ii::core::fail(ii::core::ErrorCode::busy, "Application scene is still active");
        if (slot.generation == std::numeric_limits<std::uint64_t>::max())
            return ii::core::fail(ii::core::ErrorCode::full, "Application scene generation is exhausted");
        if (slot.phase == Phase::active) --active_;
        ++slot.generation;
        slot.phase = Phase::active;
        slot.guard = std::move(guard);
        ++active_;
        return Ticket{index, slot.generation};
    }

    [[nodiscard]] ii::core::Result<bool> request_close(Ticket ticket) {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        auto* slot = find(ticket);
        if (!accepting_ || deciding_ || !slot || slot->phase != Phase::active) return false;
        deciding_ = true;
        struct Reset final { bool& value; ~Reset() { value = false; } } reset{deciding_};
        try {
            auto guard = slot->guard;
            if (guard) {
                auto decision = guard->request_close();
                if (!decision) return std::unexpected(decision.error());
                if (!*decision) return false;
            }
            if (!accepting_ || slot->phase != Phase::active || slot->generation != ticket.generation) return false;
            if (active_ > 1) {
                slot->phase = Phase::closing;
                --active_;
                return true;
            }
            auto decision = last_close_();
            if (!decision) return std::unexpected(decision.error());
            if (!*decision) return false;
            // A callback may synchronously report native destruction or cancel
            // admission; neither changes the accepted decision.
            if (slot->phase == Phase::active && slot->generation == ticket.generation) {
                slot->phase = Phase::closing;
                --active_;
            }
            return true;
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Last application window decision failed", std::current_exception());
        }
    }

    // Preflight every active generation before asking the application. This
    // does not close a window; the native application driver commits shutdown.
    [[nodiscard]] ii::core::Result<bool> request_application_close(LastClose application) {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        if (!application) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application close decision is missing");
        if (!accepting_ || deciding_) return false;
        deciding_ = true;
        struct Reset final { bool& value; ~Reset() { value = false; } } reset{deciding_};
        try {
            std::vector<Ticket> current;
            current.reserve(active_);
            for (std::size_t index = 0; index < slots_.size(); ++index)
                if (slots_[index].phase == Phase::active) current.push_back({index, slots_[index].generation});
            const auto stable = [&] {
                if (!accepting_ || active_ != current.size()) return false;
                for (const auto ticket : current) {
                    auto* slot = find(ticket);
                    if (!slot || slot->phase != Phase::active) return false;
                }
                return true;
            };
            for (const auto ticket : current) {
                auto* slot = find(ticket);
                auto guard = slot->guard;
                if (guard) {
                    auto decision = guard->request_close();
                    if (!decision) return std::unexpected(decision.error());
                    if (!*decision) return false;
                }
                if (!stable()) return false;
            }
            auto decision = application();
            if (!decision) return std::unexpected(decision.error());
            return *decision && stable();
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Application window close approval failed", std::current_exception());
        }
    }

    // The boolean identifies an unexpected native destruction that left no
    // active windows. It is never emitted for an approved close or rollback.
    [[nodiscard]] ii::core::Result<bool> closed(Ticket ticket) {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        auto* slot = find(ticket);
        if (!slot || slot->phase == Phase::closed) return false;
        const bool unexpected = slot->phase == Phase::active;
        if (unexpected) --active_;
        slot->phase = Phase::closed;
        slot->guard.reset();
        return unexpected && !active_ && accepting_;
    }

    // A failed show is not a native close and must not request app termination.
    [[nodiscard]] ii::core::Result<void> abandon(Ticket ticket) {
        if (auto owner = on_owner(); !owner) return owner;
        auto* slot = find(ticket);
        if (!slot || slot->phase == Phase::closed) return {};
        if (slot->phase == Phase::active) --active_;
        slot->phase = Phase::closed;
        slot->guard.reset();
        return {};
    }
    [[nodiscard]] ii::core::Result<void> cancel() {
        if (auto owner = on_owner(); !owner) return owner;
        accepting_ = false;
        return {};
    }
    [[nodiscard]] ii::core::Result<std::size_t> active_count() const {
        if (auto owner = on_owner(); !owner) return std::unexpected(owner.error());
        return active_;
    }

private:
    static std::size_t valid_count(std::size_t count) {
        if (!count || count > maximum_scenes)
            throw std::invalid_argument("Invalid application scene count");
        return count;
    }
    enum class Phase { closed, active, closing };
    struct Slot final { std::uint64_t generation = 0; Phase phase = Phase::closed; std::shared_ptr<WindowCloseGuard> guard; };
    Slot* find(Ticket ticket) {
        if (!ticket.generation || ticket.index >= slots_.size()) return nullptr;
        auto& slot = slots_[ticket.index];
        return slot.generation == ticket.generation ? &slot : nullptr;
    }
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scenes require their owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    std::vector<Slot> slots_;
    LastClose last_close_;
    std::size_t active_ = 0;
    bool accepting_ = true, deciding_ = false;
};

} // namespace lvui::native
