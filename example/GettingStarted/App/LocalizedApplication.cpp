#include "LocalizedApplication.generated.h"
#include <LVUI/NativeStringCatalog.h>
#include "StringCatalogAdapters.h"
#include <filesystem>
#include <memory>

// The example target supplies this development resource directory. A shipped
// application selects its own bundle or storage-provider resource location.
ii::core::Result<void> ii::runtime::configure_application(Application& app) {
    auto strings=std::make_shared<lvui::native::AppStrings>();
    const std::filesystem::path resources{LVUI_EXAMPLE_STRINGS_ROOT};
    const auto storage=lvui_example::catalog_storage();
    const auto codec=lvui_example::catalog_codec();
    for(const auto* filename:{"en.json","ko.json"})
        if(auto loaded=lvui::native::load_string_catalog(*strings,resources/filename,storage,codec);!loaded)return loaded;
    const auto factories=lvui::native::BackendFactories{}.with_app_strings(std::move(strings));
    return lvui_generated::LocalizedApplication::configure_application(app,{},factories);
}
