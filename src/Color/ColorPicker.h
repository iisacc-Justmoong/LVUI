#pragma once

#include <LVUI/ColorSelection.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {

enum class ColorPickerMode { wheel, hue_saturation, saturation_brightness, grayscale, rgb, cmyk };

struct ColorPickerConfiguration final {
    ColorPickerMode mode = ColorPickerMode::wheel;
    double width = 320, padding = 12;
    bool enabled = true, show_recent_colors = true, show_actions = true, remember_on_accept = true;
    std::string apply_text = "Apply", cancel_text = "Cancel", accessibility_name = "Color picker";
    std::vector<StyleColor> recent_colors;
    friend bool operator==(const ColorPickerConfiguration&, const ColorPickerConfiguration&) = default;
};

// The picker owns only its edit session and configuration. The color editor
// is replaceable; a surface or another consumer can share that editor.
class ColorPicker final {
public:
    explicit ColorPicker(std::shared_ptr<ColorEditing> editor) : editor_(std::move(editor)) {
        if (!editor_) throw std::invalid_argument("Color picker requires an editor");
    }
    ColorPicker(const ColorPicker&) = delete;
    ColorPicker& operator=(const ColorPicker&) = delete;
    ColorPicker(ColorPicker&&) = delete;
    ColorPicker& operator=(ColorPicker&&) = delete;
    ~ColorPicker() {
        if (token_) try { (void)editor_->complete(*token_, false); } catch (...) {}
    }

    [[nodiscard]] static bool validate(const ColorPickerConfiguration& value) noexcept {
        if (value.mode != ColorPickerMode::wheel && value.mode != ColorPickerMode::hue_saturation &&
            value.mode != ColorPickerMode::saturation_brightness && value.mode != ColorPickerMode::grayscale &&
            value.mode != ColorPickerMode::rgb && value.mode != ColorPickerMode::cmyk) return false;
        if (!std::isfinite(value.width) || value.width < 320 || value.width > 1024 ||
            !std::isfinite(value.padding) || value.padding < 0 || value.padding > 64 || value.width - 2 * value.padding < 228 ||
            !valid_accessibility_text(value.apply_text) || !valid_accessibility_text(value.cancel_text) ||
            !valid_accessibility_text(value.accessibility_name) || value.recent_colors.size() > 10) return false;
        return std::ranges::all_of(value.recent_colors, valid_color_value);
    }

    bool set_configuration(ColorPickerConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }
    [[nodiscard]] const ColorPickerConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] const std::shared_ptr<ColorEditing>& editor() const noexcept { return editor_; }
    [[nodiscard]] bool active() const { return token_ && editor_->snapshot().editing == token_; }

    [[nodiscard]] ColorSelectionResult<void> set_editor(std::shared_ptr<ColorEditing> editor) {
        if (!editor) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker requires an editor"});
        if (editor == editor_) return {};
        if (active()) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::busy, "Complete the current color edit before changing editors"});
        token_.reset(); editor_ = std::move(editor); return {};
    }

    [[nodiscard]] ColorSelectionResult<ColorEditToken> begin() {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (active()) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::busy, "Color picker is already editing"});
        token_.reset(); auto started = editor_->begin();
        if (!started) return std::unexpected(started.error());
        token_ = *started; return *started;
    }

    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> begin_if_current(std::uint64_t revision) {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (active()) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::busy, "Color picker is already editing"});
        token_.reset();
        auto started = editor_->begin_if_current(revision);
        if (!started) return std::unexpected(started.error());
        if (!started->editing || started->revision <= revision || started->baseline != started->color ||
            !valid_color_value(started->color)) {
            if (started->editing) (void)editor_->complete(*started->editing, false);
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color editor returned an invalid edit start"});
        }
        token_ = started->editing;
        return *started;
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> set_color(StyleColor color) {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        auto result = editor_->edit(*token_, color);
        if (!result) forget_stale(result.error());
        return result;
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> set_color_if_current(std::uint64_t revision, StyleColor color) {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        const auto current = editor_->snapshot();
        if (current.editing != token_ || current.revision != revision) {
            if (current.editing != token_) token_.reset();
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker input is stale"});
        }
        auto result = editor_->edit_if_current(revision, color, false);
        if (!result) forget_stale(result.error());
        return result;
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> set_hex(std::string_view input) {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        auto current = editor_->snapshot();
        if (current.editing != token_) { token_.reset(); return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color edit session is stale"}); }
        return set_hex_if_current(current.revision, input);
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> set_hex_if_current(std::uint64_t revision, std::string_view input) {
        if (!configuration_.enabled) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color picker is disabled"});
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        auto current = editor_->snapshot();
        if (current.editing != token_ || current.revision != revision) {
            if (current.editing != token_) token_.reset();
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker input is stale"});
        }
        auto color = parse_hex(input, current.color.alpha);
        if (!color) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Expected six or eight hexadecimal color digits"});
        auto result = editor_->edit_if_current(revision, *color, false);
        if (!result) forget_stale(result.error());
        return result;
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> select_recent(std::size_t index) {
        if (index >= configuration_.recent_colors.size()) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Recent color index is out of range"});
        return set_color(configuration_.recent_colors[index]);
    }

    [[nodiscard]] ColorSelectionResult<std::uint64_t> select_recent_if_current(std::uint64_t revision, std::size_t index) {
        if (index >= configuration_.recent_colors.size()) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Recent color index is out of range"});
        return set_color_if_current(revision, configuration_.recent_colors[index]);
    }

    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> accept() {
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        std::vector<StyleColor> recent;
        if (configuration_.remember_on_accept) { recent = configuration_.recent_colors; recent.reserve(11); }
        auto completed = editor_->complete(*token_, true);
        if (!completed) { forget_stale(completed.error()); return std::unexpected(completed.error()); }
        token_.reset();
        if (!valid_color_value(completed->color) || completed->editing) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color editor returned an invalid completion"});
        }
        if (configuration_.remember_on_accept) {
            std::erase(recent, completed->color);
            recent.insert(recent.begin(), completed->color);
            if (recent.size() > 10) recent.resize(10);
            configuration_.recent_colors = std::move(recent);
        }
        return *completed;
    }

    [[nodiscard]] ColorSelectionResult<ColorSelectionSnapshot> cancel() {
        if (!token_) return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::stale, "Color picker has no active edit"});
        auto completed = editor_->complete(*token_, false);
        if (!completed) { forget_stale(completed.error()); return std::unexpected(completed.error()); }
        token_.reset();
        if (!valid_color_value(completed->color) || completed->editing) {
            return std::unexpected(ColorSelectionError{ColorSelectionErrorCode::invalid, "Color editor returned an invalid completion"});
        }
        return *completed;
    }

    [[nodiscard]] static std::optional<StyleColor> parse_hex(std::string_view value, double alpha) noexcept {
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) return std::nullopt;
        if (!value.empty() && value.front() == '#') value.remove_prefix(1);
        if (value.size() != 6 && value.size() != 8) return std::nullopt;
        const auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        unsigned bytes[4]{0, 0, 0, 0};
        for (std::size_t index = 0; index < value.size() / 2; ++index) {
            const auto high = digit(value[index * 2]), low = digit(value[index * 2 + 1]);
            if (high < 0 || low < 0) return std::nullopt;
            bytes[index] = unsigned(high * 16 + low);
        }
        return StyleColor{bytes[0] / 255., bytes[1] / 255., bytes[2] / 255., value.size() == 8 ? bytes[3] / 255. : alpha};
    }

    [[nodiscard]] static std::string format_hex(StyleColor color) {
        if (!valid_color_value(color)) throw std::invalid_argument("Invalid color");
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result = "#00000000";
        const double channels[]{color.red, color.green, color.blue, color.alpha};
        for (std::size_t index = 0; index < 4; ++index) {
            const auto value = unsigned(std::lround(channels[index] * 255));
            result[index * 2 + 1] = digits[value >> 4]; result[index * 2 + 2] = digits[value & 15];
        }
        return result;
    }

private:
    void forget_stale(const ColorSelectionError& error) {
        if (error.code == ColorSelectionErrorCode::stale && token_ && editor_->snapshot().editing != token_) token_.reset();
    }
    std::shared_ptr<ColorEditing> editor_;
    ColorPickerConfiguration configuration_;
    std::optional<ColorEditToken> token_;
};

} // namespace lvui
