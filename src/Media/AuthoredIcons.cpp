#include <LVUI/AuthoredIcons.h>
#include "AuthoredIconData.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace lvui {
namespace {
std::string normalize(std::string_view raw) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto begin = raw.find_first_not_of(whitespace);
    if (begin == raw.npos) return {};
    raw = raw.substr(begin, raw.find_last_not_of(whitespace) - begin + 1);
    constexpr std::string_view previous_resource_prefix = "qrc:/qt/qml/LVRS/resources/iconset/";
    if (raw.starts_with(previous_resource_prefix)) raw.remove_prefix(previous_resource_prefix.size());
    std::string value(raw);
    auto fold = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
    auto lower = value;
    std::ranges::transform(lower, lower.begin(), fold);
    if (lower.ends_with(".svg")) value.resize(value.size() - 4);
    std::erase(value, '/');
    lower = value;
    std::ranges::transform(lower, lower.begin(), fold);
    const auto aliases = detail::authored_name_aliases();
    const auto found = std::ranges::lower_bound(aliases, lower, {}, &detail::AuthoredIconAlias::first);
    if (found != aliases.end() && found->first == lower) value = found->second;
    return value;
}

std::string canonical_name(std::string_view raw) {
    auto key = normalize(raw);
    const auto aliases = detail::authored_resource_aliases();
    const auto alias = std::ranges::lower_bound(aliases, key, {}, &detail::AuthoredIconAlias::first);
    if (alias != aliases.end() && alias->first == key) key = alias->second;
    return key;
}

class AuthoredIconCatalog final : public IconSource {
  public:
    [[nodiscard]] IconResult<std::shared_ptr<const IconAsset>> resolve(std::string_view name) const override {
        if (name.empty() || name.size() > 256 || !valid_accessibility_text(name))
            return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid authored icon name"});
        auto key = canonical_name(name);
        if (key.empty()) return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid authored icon name"});
        const auto records = detail::authored_icon_records();
        auto record = std::ranges::lower_bound(records, key, {}, &detail::AuthoredIconRecord::name);
        if (record == records.end() || record->name != key)
            return std::unexpected(IconError{IconErrorCode::not_found, "Authored icon was not found"});
        std::lock_guard lock(mutex_);
        auto found = cached_.find(record->name);
        if (found != cached_.end()) return found->second;
        auto asset = std::make_shared<const IconAsset>(IconAsset{std::string(record->svg)});
        cached_.emplace(record->name, asset);
        return asset;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return 1; }
  private:
    mutable std::mutex mutex_;
    mutable std::map<std::string_view, std::shared_ptr<const IconAsset>, std::less<>> cached_;
};

class DesignIconCatalog final : public IconSource {
  public:
    [[nodiscard]] IconResult<std::shared_ptr<const IconAsset>> resolve(std::string_view name) const override {
        if (name.empty() || name.size() > 256 || !valid_accessibility_text(name))
            return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid design icon name"});
        const auto key = canonical_name(name);
        if (key.empty()) return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid design icon name"});
        const auto records = detail::design_icon_records();
        const auto record = std::ranges::lower_bound(records, key, {}, &detail::AuthoredIconRecord::name);
        if (record == records.end() || record->name != key)
            return authored_icon_catalog()->resolve(name);
        std::lock_guard lock(mutex_);
        auto found = cached_.find(record->name);
        if (found != cached_.end()) return found->second;
        auto asset = std::make_shared<const IconAsset>(IconAsset{std::string(record->svg)});
        cached_.emplace(record->name, asset);
        return asset;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return 1; }
  private:
    mutable std::mutex mutex_;
    mutable std::map<std::string_view, std::shared_ptr<const IconAsset>, std::less<>> cached_;
};
} // namespace

std::shared_ptr<const IconSource> authored_icon_catalog() {
    static const auto catalog = std::make_shared<const AuthoredIconCatalog>();
    return catalog;
}
std::shared_ptr<const IconSource> design_icon_catalog() {
    static const auto catalog = std::make_shared<const DesignIconCatalog>();
    return catalog;
}
std::span<const std::string_view> design_icon_names() noexcept {
    return detail::design_icon_record_names();
}
IconResult<std::string_view> design_icon_svg(std::string_view name) {
    if (name.empty() || name.size() > 256 || !valid_accessibility_text(name))
        return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid design icon name"});
    const auto records = detail::design_icon_records();
    const auto found = std::ranges::lower_bound(records, name, {}, &detail::AuthoredIconRecord::name);
    if (found == records.end() || found->name != name)
        return std::unexpected(IconError{IconErrorCode::not_found, "Design icon was not found"});
    return found->svg;
}
std::span<const std::string_view> authored_icon_names() noexcept {
    return detail::authored_icon_record_names();
}
IconResult<std::string_view> authored_icon_svg(std::string_view name) {
    if (name.empty() || name.size() > 256 || !valid_accessibility_text(name))
        return std::unexpected(IconError{IconErrorCode::invalid_argument, "Invalid authored icon name"});
    const auto records = detail::authored_icon_records();
    const auto found = std::ranges::lower_bound(records, name, {}, &detail::AuthoredIconRecord::name);
    if (found == records.end() || found->name != name)
        return std::unexpected(IconError{IconErrorCode::not_found, "Authored icon was not found"});
    return found->svg;
}
std::optional<std::string_view> detail::authored_icon_render_fallback(
    const std::shared_ptr<const IconAsset>& asset) {
    if (!asset) return std::nullopt;
    static const auto fallbacks = [] {
        std::vector<std::pair<std::shared_ptr<const IconAsset>, std::string_view>> result;
        const auto catalog = authored_icon_catalog();
        for (const auto& entry : authored_icon_fallbacks()) {
            auto item = catalog->resolve(entry.name);
            if (item && *item) result.emplace_back(*item, entry.svg);
        }
        return result;
    }();
    for (const auto& [original, svg] : fallbacks)
        if (original == asset) return svg;
    return std::nullopt;
}
} // namespace lvui
