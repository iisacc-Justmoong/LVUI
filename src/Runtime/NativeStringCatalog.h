#pragma once
#include <LVUI/NativeAppStrings.h>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui::native {
struct StringCatalog final {
    std::string locale;
    AppStrings::Entries strings;
};

// An application supplies its own format and file provider; neither the LVUI
// runtime nor this contract links a parser, storage provider or GUI module.
struct StringCatalogCodec final {
    std::function<ii::core::Result<StringCatalog>(std::string_view)> decode;
    std::function<ii::core::Result<std::string>(const StringCatalog&)> encode;
};
struct StringCatalogStorage final {
    std::function<ii::core::Result<std::string>(const std::filesystem::path&,std::size_t)> read;
    std::function<ii::core::Result<void>(const std::filesystem::path&,std::string_view)> write;
};

// Validate untrusted adapter output before publishing a catalog. A failed
// read, decode or validation never mutates the active string store.
[[nodiscard]] inline ii::core::Result<StringCatalog> decode_string_catalog(
        std::string_view input,const StringCatalogCodec& codec) {
    if(!codec.decode)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog decoder is required");
    if(input.size()>AppStrings::maximum_catalog_bytes)
        return ii::core::fail(ii::core::ErrorCode::out_of_range,"String catalog exceeds its byte limit");
    try {
        auto catalog=codec.decode(input);if(!catalog)return std::unexpected(catalog.error());
        auto locale=AppStrings::normalize_locale(catalog->locale);
        if(!locale)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid string catalog locale");
        auto valid=AppStrings::validate(catalog->strings);if(!valid)return std::unexpected(valid.error());
        catalog->locale=std::move(*locale);return catalog;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"String catalog decoding failed",std::current_exception());}
}
[[nodiscard]] inline ii::core::Result<std::string> encode_string_catalog(
        const StringCatalog& catalog,const StringCatalogCodec& codec) {
    if(!codec.encode)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog encoder is required");
    try {
        auto locale=AppStrings::normalize_locale(catalog.locale);
        if(!locale)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid string catalog locale");
        auto valid=AppStrings::validate(catalog.strings);if(!valid)return std::unexpected(valid.error());
        auto output=[&] {
            if(*locale==catalog.locale)return codec.encode(catalog);
            return codec.encode(StringCatalog{std::move(*locale),catalog.strings});
        }();
        if(!output)return std::unexpected(output.error());
        if(output->size()>AppStrings::maximum_catalog_bytes)
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Encoded string catalog exceeds its byte limit");
        return output;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"String catalog encoding failed",std::current_exception());}
}
[[nodiscard]] inline ii::core::Result<void> load_string_catalog(AppStrings& strings,
        const std::filesystem::path& path,const StringCatalogStorage& storage,const StringCatalogCodec& codec) {
    if(!storage.read)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog reader is required");
    try {
        auto bytes=storage.read(path,AppStrings::maximum_catalog_bytes);if(!bytes)return std::unexpected(bytes.error());
        auto catalog=decode_string_catalog(*bytes,codec);if(!catalog)return std::unexpected(catalog.error());
        return strings.replace_catalog(catalog->locale,std::move(catalog->strings));
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot read string catalog",std::current_exception());}
}
[[nodiscard]] inline ii::core::Result<void> save_string_catalog(const AppStrings& strings,
        std::string_view locale,const std::filesystem::path& path,const StringCatalogStorage& storage,
        const StringCatalogCodec& codec) {
    if(!storage.write)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog writer is required");
    try {
        auto values=strings.catalog(locale);if(!values)return std::unexpected(values.error());
        auto bytes=encode_string_catalog({std::string(locale),std::move(*values)},codec);
        if(!bytes)return std::unexpected(bytes.error());
        return storage.write(path,*bytes);
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot save string catalog",std::current_exception());}
}
} // namespace lvui::native
