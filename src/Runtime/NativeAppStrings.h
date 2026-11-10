#pragma once

#include <LVUI/NativeBindings.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cstddef>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

// One owner-thread catalog per application. Snapshots preserve the values
// read by an in-progress binding while a reentrant update replaces a catalog.
class AppStrings final {
public:
    using Entries = std::map<std::string, std::string, std::less<>>;
    static constexpr std::size_t maximum_locales = 128, maximum_entries = 10000,
        maximum_key_bytes = 512, maximum_value_bytes = 65536,
        maximum_catalog_bytes = 8 * 1024 * 1024, maximum_total_bytes = 32 * 1024 * 1024;
private:
    struct Catalog final { Entries entries; std::size_t bytes = 0; };
    struct State final {
        std::string locale = "en", fallback = "en";
        std::map<std::string, std::shared_ptr<const Catalog>, std::less<>> catalogs;
        std::size_t bytes = 0;
    };
public:
    AppStrings() : state_(std::make_shared<const State>()) {}
    AppStrings(const AppStrings&) = delete;
    AppStrings& operator=(const AppStrings&) = delete;

    [[nodiscard]] static std::optional<std::string> normalize_locale(std::string_view value) {
        if (value.empty() || value.size() > 63) return {};
        std::string result; result.reserve(value.size());
        std::size_t component = 0;
        for (const auto c : value) {
            if (c == '-') {
                if (!component || component > 8) return {};
                component = 0; result.push_back(c); continue;
            }
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return {};
            result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
            ++component;
        }
        if (!component || component > 8) return {};
        return result;
    }
    [[nodiscard]] static ii::core::Result<std::size_t> validate(const Entries& values) {
        if (values.size() > maximum_entries) return ii::core::fail(ii::core::ErrorCode::full, "String catalog has too many entries");
        std::size_t bytes = 0;
        for (const auto& [key, value] : values) {
            if (!valid_key(key) || value.size() > maximum_value_bytes || !valid_text(value))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "String catalog requires valid bounded UTF-8 keys and values");
            if (key.size() + value.size() > maximum_catalog_bytes - bytes)
                return ii::core::fail(ii::core::ErrorCode::full, "String catalog is too large");
            bytes += key.size() + value.size();
        }
        return bytes;
    }
    [[nodiscard]] ii::core::Result<void> replace_catalog(std::string_view locale, Entries values) {
        if (auto checked=check(); !checked) return checked;
        const auto normalized=normalize_locale(locale);
        if (!normalized) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid locale tag");
        const auto bytes=validate(values); if (!bytes) return std::unexpected(bytes.error());
        try {
            auto current=state_.get();
            const auto previous=current->catalogs.find(*normalized);
            if (previous!=current->catalogs.end() && previous->second->entries==values) return {};
            if (previous==current->catalogs.end() && current->catalogs.size()>=maximum_locales)
                return ii::core::fail(ii::core::ErrorCode::full, "Too many string catalog locales");
            const auto old=previous==current->catalogs.end()?0:previous->second->bytes;
            if (*bytes > maximum_total_bytes-(current->bytes-old))
                return ii::core::fail(ii::core::ErrorCode::full, "App string storage is full");
            auto next=std::make_shared<State>(*current);
            next->catalogs.insert_or_assign(*normalized,std::make_shared<const Catalog>(Catalog{std::move(values),*bytes}));
            next->bytes=current->bytes-old+*bytes;
            state_.set(std::move(next)); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "String catalog update failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> set_locale(std::string_view value) { return change_locale(value,false); }
    [[nodiscard]] ii::core::Result<void> set_fallback_locale(std::string_view value) { return change_locale(value,true); }
    [[nodiscard]] ii::core::Result<void> set_string(std::string_view key, std::string_view value) {
        if (auto checked=check(); !checked) return checked;
        if (!valid_key(key) || value.size()>maximum_value_bytes || !valid_text(value))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid UTF-8 string key or value");
        try {
            const auto current=state_.get();
            const auto found=current->catalogs.find(current->locale);
            Entries next=found==current->catalogs.end()?Entries{}:found->second->entries;
            next.insert_or_assign(std::string(key),std::string(value));
            return replace_catalog(current->locale,std::move(next));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "String replacement failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<Entries> catalog(std::string_view locale) const {
        if (auto checked=check(); !checked) return std::unexpected(checked.error());
        const auto normalized=normalize_locale(locale);
        if (!normalized) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid locale tag");
        try {
            const auto current=state_.get(); const auto found=current->catalogs.find(*normalized);
            if (found==current->catalogs.end()) return ii::core::fail(ii::core::ErrorCode::not_found, "String catalog locale not found");
            return found->second->entries;
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "String catalog read failed", std::current_exception()); }
    }
    [[nodiscard]] std::string locale() const { return state_.get()->locale; }
    [[nodiscard]] std::string get(std::string_view key) const { return lookup(*state_.get(),key); }
    [[nodiscard]] std::string read(ReadContext& context, std::string_view key) const { return lookup(*context.read(state_),key); }
private:
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {
        return value.find('\0')==value.npos && ii::core::valid_utf8(value);
    }
    [[nodiscard]] static bool valid_key(std::string_view value) noexcept {
        return !value.empty() && value.size()<=maximum_key_bytes && valid_text(value);
    }
    [[nodiscard]] ii::core::Result<void> check() const {
        if (owner_!=std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "App strings require their UI thread");
        return {};
    }
    [[nodiscard]] ii::core::Result<void> change_locale(std::string_view value, bool fallback) {
        if (auto checked=check(); !checked) return checked;
        const auto normalized=normalize_locale(value);
        if (!normalized) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid locale tag");
        try {
            const auto current=state_.get();
            if ((fallback?current->fallback:current->locale)==*normalized) return {};
            auto next=std::make_shared<State>(*current);
            (fallback?next->fallback:next->locale)=*normalized;
            state_.set(std::move(next)); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Locale update failed", std::current_exception()); }
    }
    [[nodiscard]] static std::string lookup(const State& state, std::string_view key) {
        if (!valid_key(key)) throw std::invalid_argument("Localized string requires a valid bounded UTF-8 key");
        const auto search=[&](std::string locale)->std::optional<std::string>{
            for (;;) {
                if (auto catalog=state.catalogs.find(locale);catalog!=state.catalogs.end())
                    if (auto value=catalog->second->entries.find(key);value!=catalog->second->entries.end()) return value->second;
                const auto separator=locale.rfind('-');if(separator==locale.npos)break;locale.resize(separator);
            }
            return {};
        };
        if (auto value=search(state.locale)) return *value;
        if (auto value=search(state.fallback)) return *value;
        return std::string(key);
    }
    const std::thread::id owner_=std::this_thread::get_id();
    ii::core::Observable<std::shared_ptr<const State>> state_;
};

} // namespace lvui::native
