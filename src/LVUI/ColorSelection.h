#pragma once

#include "ColorSource.h"

#include <atomic>
#include <cstdint>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

namespace lvui {

enum class ColorSelectionErrorCode { invalid, busy, stale, exhausted };

struct ColorSelectionError final {
    ColorSelectionErrorCode code;
    std::string message;
};

template<class T>
using ColorSelectionResult = std::expected<T, ColorSelectionError>;

struct ColorEditToken final {
    std::uint64_t value = 0;
    friend bool operator==(const ColorEditToken&, const ColorEditToken&) = default;
};

enum class ColorEditTermination { accepted, canceled, replaced };
struct ColorEditCompletion final {
    ColorEditToken token;
    ColorEditTermination termination;
    friend bool operator==(const ColorEditCompletion&, const ColorEditCompletion&) = default;
};

struct ColorSelectionSnapshot final {
    StyleColor color, baseline;
    std::uint64_t revision = 0;
    std::optional<ColorEditToken> editing;
    std::optional<ColorEditCompletion> last_completion;
};

// A picker may use any application-supplied editor with the same transaction
// and revision contract, independently of its color-space implementation.
class ColorEditing : public ColorSource {
public:
    [[nodiscard]] virtual ColorSelectionSnapshot snapshot() const = 0;
    [[nodiscard]] virtual ColorSelectionResult<ColorEditToken> begin() = 0;
    // Start only if the observed selection is still current, and return the
    // resulting token, baseline and revision in the same atomic snapshot.
    [[nodiscard]] virtual ColorSelectionResult<ColorSelectionSnapshot> begin_if_current(std::uint64_t) = 0;
    [[nodiscard]] virtual ColorSelectionResult<std::uint64_t> edit(ColorEditToken, StyleColor) = 0;
    [[nodiscard]] virtual ColorSelectionResult<std::uint64_t> edit_if_current(
        std::uint64_t, StyleColor, bool representation_changed = false) = 0;
    // The returned color and revision describe the same atomic completion.
    [[nodiscard]] virtual ColorSelectionResult<ColorSelectionSnapshot> complete(ColorEditToken, bool accepted) = 0;
};

// A session owns its rollback baseline. External replacement invalidates the
// token so a late cancel can never overwrite a newer application color.
class ColorSelection final : public ColorEditing {
public:
    explicit ColorSelection(StyleColor initial = {.5275065302848816, .291259765625, 1, 1})
        : color_(initial) {
        if (!valid_color_value(initial)) throw std::invalid_argument("Invalid initial color");
    }

    [[nodiscard]] ColorSourceResult color() const override {
        std::scoped_lock lock(mutex_);
        return color_;
    }

    [[nodiscard]] std::uint64_t revision() const noexcept override {
        return revision_.load(std::memory_order_acquire);
    }

    [[nodiscard]] ColorSelectionSnapshot snapshot() const override {
        std::scoped_lock lock(mutex_);
        return {color_, session_ ? session_->baseline : color_,
                revision_.load(std::memory_order_relaxed),
                session_ ? std::optional<ColorEditToken>{session_->token} : std::nullopt,
                last_completion_};
    }

    [[nodiscard]] ColorSelectionResult<ColorEditToken> begin() override {
        std::scoped_lock lock(mutex_);
        auto started = start_locked();
        if (!started) return std::unexpected(started.error());
        return *started->editing;
    }

    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> begin_if_current(std::uint64_t expected) override {
        std::scoped_lock lock(mutex_);
        if (revision_.load(std::memory_order_relaxed) != expected) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale,
                                                       "Color selection changed before the edit began"});
        }
        return start_locked();
    }

private:
    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> start_locked() {
        if (session_) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::busy,
                                                       "Color edit session is already active"});
        }
        if (next_token_ == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::exhausted,
                                                       "Color edit token exhausted"});
        }
        if (auto allowed = revision_available(); !allowed) return std::unexpected(allowed.error());
        const ColorEditToken token{++next_token_};
        session_ = Session{token, color_};
        const auto revision = changed();
        return ColorSelectionSnapshot{color_, color_, revision, token, last_completion_};
    }

public:
    [[nodiscard]] ColorSelectionResult<std::uint64_t> edit(ColorEditToken token, StyleColor value) override {
        if (!valid_color_value(value)) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Invalid color"});
        }
        std::scoped_lock lock(mutex_);
        if (auto current = active(token); !current) return std::unexpected(current.error());
        if (value == color_) return revision_.load(std::memory_order_relaxed);
        if (auto allowed = revision_available(); !allowed) return std::unexpected(allowed.error());
        color_ = value;
        return changed();
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> accept(ColorEditToken token) {
        auto result = complete(token, true);
        if (!result) return std::unexpected(result.error());
        return result->revision;
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> cancel(ColorEditToken token) {
        auto result = complete(token, false);
        if (!result) return std::unexpected(result.error());
        return result->revision;
    }

    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> complete(ColorEditToken token, bool accepted) override {
        std::scoped_lock lock(mutex_);
        if (auto current = active(token); !current) return std::unexpected(current.error());
        if (auto allowed = revision_available(); !allowed) return std::unexpected(allowed.error());
        last_completion_ = ColorEditCompletion{token, accepted ? ColorEditTermination::accepted : ColorEditTermination::canceled};
        if (!accepted) color_ = session_->baseline;
        session_.reset();
        const auto revision = changed();
        return ColorSelectionSnapshot{color_, color_, revision, std::nullopt, last_completion_};
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> set_color(StyleColor value) {
        if (!valid_color_value(value)) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Invalid color"});
        }
        std::scoped_lock lock(mutex_);
        if (value == color_ && !session_) return revision_.load(std::memory_order_relaxed);
        if (auto allowed = revision_available(); !allowed) return std::unexpected(allowed.error());
        if (session_) last_completion_ = ColorEditCompletion{session_->token, ColorEditTermination::replaced};
        color_ = value;
        session_.reset();
        return changed();
    }

    // A surface may edit the current selection without taking ownership of
    // the session. The revision prevents a delayed gesture from overwriting a
    // newer color or a newly opened session. A color surface may also publish
    // a change to its latent hue when the resulting RGB value is unchanged.
    [[nodiscard]] ColorSelectionResult<std::uint64_t> edit_if_current(
            std::uint64_t expected_revision, StyleColor value, bool representation_changed = false) override {
        if (!valid_color_value(value)) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Invalid color"});
        }
        std::scoped_lock lock(mutex_);
        if (revision_.load(std::memory_order_relaxed) != expected_revision) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale,
                                                       "Color selection changed during the edit"});
        }
        if (value == color_ && !representation_changed) return expected_revision;
        if (auto allowed = revision_available(); !allowed) return std::unexpected(allowed.error());
        color_ = value;
        return changed();
    }

private:
    struct Session final {
        ColorEditToken token;
        StyleColor baseline;
    };

    // These helpers are called while mutex_ is held.
    [[nodiscard]] ColorSelectionResult<void> revision_available() const {
        if (revision_.load(std::memory_order_relaxed) == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::exhausted,
                                                       "Color selection revision exhausted"});
        }
        return {};
    }

    [[nodiscard]] ColorSelectionResult<void> active(ColorEditToken token) const {
        if (!session_ || session_->token != token) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale,
                                                       "Color edit session is stale"});
        }
        return {};
    }

    std::uint64_t changed() noexcept {
        return revision_.fetch_add(1, std::memory_order_release) + 1;
    }

    mutable std::mutex mutex_;
    StyleColor color_;
    std::optional<Session> session_;
    std::optional<ColorEditCompletion> last_completion_;
    std::uint64_t next_token_ = 0;
    std::atomic<std::uint64_t> revision_{0};
};

} // namespace lvui
