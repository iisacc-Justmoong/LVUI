#pragma once
#include <LVUI/IconCatalog.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

class NativeNamedIconSource final : public lvui::IconSource {
    std::shared_ptr<const lvui::IconAsset> asset_=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10' fill='#ff0000'/></svg>"});
    std::uint64_t revision_=1;
public:
    [[nodiscard]] lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view name) const override {
        if(name!="brand")return std::unexpected(lvui::IconError{lvui::IconErrorCode::not_found,"Unknown custom icon"});
        return asset_;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept override{return revision_;}
    void replace(std::string svg){asset_=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{std::move(svg)});++revision_;}
};
struct NativeOwnedNamedIconsModel final {
    std::shared_ptr<NativeNamedIconSource> owner=std::make_shared<NativeNamedIconSource>();
    ii::core::Observable<std::shared_ptr<const lvui::IconSource>> source{owner};
    ii::core::Observable<std::string> name{std::string("brand")};
    ii::core::Observable<std::int64_t> revision{1};
};
