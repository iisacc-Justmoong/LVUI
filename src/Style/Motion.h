#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace lvui {

enum class MotionCurve { linear, out_back };

// A value supplied by an application, not a dependency on a host scheduler.
struct Motion final {
    std::chrono::milliseconds duration{360};
    MotionCurve curve = MotionCurve::out_back;
    double overshoot = 1.45;
    bool reduced_motion = false;
    friend bool operator==(const Motion&, const Motion&) = default;
};

[[nodiscard]] inline bool valid_motion(const Motion& value) noexcept {
    return value.duration >= std::chrono::milliseconds::zero() &&
           value.duration <= std::chrono::hours(24) &&
           (value.curve == MotionCurve::linear || value.curve == MotionCurve::out_back) &&
           std::isfinite(value.overshoot) && value.overshoot >= 0 && value.overshoot <= 10;
}

[[nodiscard]] inline Motion motion_value(const Motion& value) noexcept { return value; }
template<class T> requires std::same_as<std::remove_const_t<T>, Motion>
[[nodiscard]] Motion motion_value(const std::shared_ptr<T>& value) noexcept {
    return value ? *value : Motion{};
}
template<class T> requires std::same_as<std::remove_const_t<T>, Motion>
[[nodiscard]] Motion motion_value(const std::weak_ptr<T>& value) noexcept {
    return motion_value(value.lock());
}

enum class MotionError { invalid_value, invalid_policy };

// Retained interpolation with an application-supplied monotonic frame time.
// It owns no timer, widget, callback or external SDK object.
template<std::size_t N> class MotionTransition final {
    static_assert(N > 0);
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Value = std::array<double, N>;

    explicit MotionTransition(Value initial = {}) : value_(initial), from_(initial), target_(initial) {
        if (!valid_value(initial)) throw std::invalid_argument("Invalid motion initial value");
    }

    [[nodiscard]] const Value& value() const noexcept { return value_; }
    [[nodiscard]] const Value& target() const noexcept { return target_; }
    [[nodiscard]] bool active() const noexcept { return active_; }

    // Invalid candidates leave both the visible value and the active motion intact.
    [[nodiscard]] std::expected<void, MotionError> retarget(Value target, Motion policy,
                                                           bool enabled, TimePoint now) noexcept {
        if (!valid_value(target)) return std::unexpected(MotionError::invalid_value);
        if (!valid_motion(policy)) return std::unexpected(MotionError::invalid_policy);
        (void)advance(now);
        if (target == target_ && policy == policy_ && enabled == enabled_) return {};
        from_ = value_;
        target_ = target;
        policy_ = policy;
        enabled_ = enabled;
        started_ = last_time_;
        active_ = enabled && !policy.reduced_motion && policy.duration.count() > 0 && from_ != target_;
        if (!active_) value_ = target_;
        return {};
    }

    // Returns whether another frame is required. Earlier timestamps never undo
    // an already-observed frame, including after a retarget.
    [[nodiscard]] bool advance(TimePoint now) noexcept {
        if (!observed_ || now > last_time_) last_time_ = now;
        observed_ = true;
        if (!active_) return false;
        using ClockDuration = typename TimePoint::duration;
        using ClockTicks = std::chrono::duration<long double, typename ClockDuration::period>;
        // Subtract counts before converting to seconds: subtracting two rounded
        // fractional seconds can leave the exact final frame just below 1.
        const auto elapsed = static_cast<long double>(last_time_.time_since_epoch().count()) -
                             static_cast<long double>(started_.time_since_epoch().count());
        const auto duration = ClockTicks(policy_.duration).count();
        const double t = static_cast<double>(std::clamp(elapsed / duration, 0.L, 1.L));
        if (t >= 1) { value_ = target_; active_ = false; return false; }
        const auto u = t - 1;
        const double factor = policy_.curve == MotionCurve::linear ? t
            : 1 + (policy_.overshoot + 1) * u * u * u + policy_.overshoot * u * u;
        for (std::size_t i = 0; i < N; ++i) value_[i] = from_[i] + (target_[i] - from_[i]) * factor;
        return true;
    }

    void settle() noexcept { value_ = from_ = target_; active_ = false; }
    [[nodiscard]] std::expected<void, MotionError> reset(Value value) noexcept {
        if (!valid_value(value)) return std::unexpected(MotionError::invalid_value);
        value_ = from_ = target_ = value;
        active_ = false;
        return {};
    }

private:
    static bool valid_value(const Value& value) noexcept {
        for (double channel : value) if (!std::isfinite(channel) || std::abs(channel) > 1e12) return false;
        return true;
    }
    Value value_, from_, target_;
    Motion policy_;
    TimePoint started_{}, last_time_{};
    bool enabled_ = true, active_ = false, observed_ = false;
};

} // namespace lvui
