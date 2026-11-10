#pragma once

// Application composition: the SDK's catalog contract is independent of the
// storage and JSON modules. An application may select these implementations.
#include <LVUI/NativeStringCatalog.h>
#include <iiFileProviderNative.h>
#include <iiJson/Json.h>

namespace lvui_example {
inline lvui::native::StringCatalogCodec catalog_codec() {
    using lvui::native::AppStrings;
    using lvui::native::StringCatalog;
    return {
        .decode=[](std::string_view input) -> ii::core::Result<StringCatalog> {
            auto parsed=ii::json::parse(input, {.input_bytes=AppStrings::maximum_catalog_bytes});
            if(!parsed)return ii::core::fail(ii::core::ErrorCode::invalid_argument,
                "String catalog JSON at "+std::to_string(parsed.error().line)+":"+
                std::to_string(parsed.error().column)+": "+parsed.error().message);
            auto root=parsed->get<ii::json::Value::Object>();
            if(!root||root->get().size()!=2||!root->get().contains("locale")||!root->get().contains("strings"))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog requires only locale and strings");
            auto language=root->get().at("locale").get<std::string>();
            auto entries=root->get().at("strings").get<ii::json::Value::Object>();
            if(!language||!entries)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog locale and strings have invalid types");
            AppStrings::Entries values;
            for(const auto& [key,value]:entries->get()){
                auto text=value.get<std::string>();
                if(!text)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"String catalog values must be strings");
                values.emplace(key,text->get());
            }
            return StringCatalog{language->get(),std::move(values)};
        },
        .encode=[](const StringCatalog& catalog) -> ii::core::Result<std::string> {
            ii::json::Value::Object entries;
            for(const auto& [key,value]:catalog.strings)entries.emplace(key,ii::json::Value(value));
            ii::json::Value::Object root;
            root.emplace("locale",ii::json::Value(catalog.locale));
            root.emplace("strings",ii::json::Value(std::move(entries)));
            auto output=ii::json::stringify(ii::json::Value(std::move(root)), {.output_bytes=AppStrings::maximum_catalog_bytes});
            if(!output)return ii::core::fail(ii::core::ErrorCode::failed,"String catalog JSON encoding failed: "+output.error().message);
            return std::move(*output);
        }};
}
inline lvui::native::StringCatalogStorage catalog_storage() {
    return {
        .read=[](const std::filesystem::path& path,std::size_t limit) -> ii::core::Result<std::string> {
            try { return iiFileProvider::native::File::read(path,limit); }
            catch(...) { return ii::core::fail(ii::core::ErrorCode::failed,"Cannot read string catalog",std::current_exception()); }
        },
        .write=[](const std::filesystem::path& path,std::string_view bytes) -> ii::core::Result<void> {
            try { iiFileProvider::native::File::write(path,std::string(bytes)); return {}; }
            catch(...) { return ii::core::fail(ii::core::ErrorCode::failed,"Cannot save string catalog",std::current_exception()); }
        }};
}
} // namespace lvui_example
