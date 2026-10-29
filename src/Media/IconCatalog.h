#pragma once
#include <LVUI/Text.h>
#include <LVUI/BuiltinIconAssets.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace lvui {
enum class IconErrorCode {invalid_argument,duplicate,limit_exceeded,not_found};
struct IconError final {IconErrorCode code;std::string message;};
template<class T>using IconResult=std::expected<T,IconError>;
struct IconAsset final {std::string svg;};
struct IconDefinition final {std::string name,svg;};
struct IconCatalogLimits final {
    std::size_t maximum_entries=4096,maximum_svg_bytes=65536,maximum_total_bytes=16*1024*1024;
};
// A source is replaceable by an application. Any mutable implementation must
// change revision whenever an existing name resolves to different bytes.
class IconSource {
public:
    virtual ~IconSource()=default;
    [[nodiscard]] virtual IconResult<std::shared_ptr<const IconAsset>> resolve(std::string_view name)const=0;
    [[nodiscard]] virtual std::uint64_t revision()const noexcept=0;
};
class IconCatalog final:public IconSource {
    std::map<std::string,std::shared_ptr<const IconAsset>,std::less<>> assets_;
    explicit IconCatalog(std::map<std::string,std::shared_ptr<const IconAsset>,std::less<>> assets):assets_(std::move(assets)){}
    [[nodiscard]] static bool valid_name(std::string_view name)noexcept{
        return !name.empty()&&name.size()<=256&&name.find_first_not_of(" \t\r\n")!=name.npos&&valid_accessibility_text(name);
    }
public:
    [[nodiscard]] static IconResult<std::shared_ptr<const IconCatalog>> create(
            std::vector<IconDefinition> entries,IconCatalogLimits limits={}){
        if(!limits.maximum_entries||limits.maximum_entries>4096||!limits.maximum_svg_bytes||limits.maximum_svg_bytes>65536||
           !limits.maximum_total_bytes||limits.maximum_total_bytes>16*1024*1024)
            return std::unexpected(IconError{IconErrorCode::invalid_argument,"Invalid icon catalog limits"});
        if(entries.size()>limits.maximum_entries)return std::unexpected(IconError{IconErrorCode::limit_exceeded,"Too many icon assets"});
        std::size_t total=0;std::map<std::string,std::shared_ptr<const IconAsset>,std::less<>> result;
        for(auto& entry:entries){
            if(!valid_name(entry.name)||entry.svg.empty()||!valid_accessibility_text(entry.svg))
                return std::unexpected(IconError{IconErrorCode::invalid_argument,"Invalid icon name or SVG text"});
            if(entry.svg.size()>limits.maximum_svg_bytes||entry.svg.size()>limits.maximum_total_bytes-total)
                return std::unexpected(IconError{IconErrorCode::limit_exceeded,"Icon byte limit exceeded"});
            total+=entry.svg.size();auto asset=std::make_shared<const IconAsset>(IconAsset{std::move(entry.svg)});
            if(!result.emplace(std::move(entry.name),std::move(asset)).second)
                return std::unexpected(IconError{IconErrorCode::duplicate,"Duplicate icon name"});
        }
        return std::shared_ptr<const IconCatalog>(new IconCatalog(std::move(result)));
    }
    [[nodiscard]] IconResult<std::shared_ptr<const IconAsset>> resolve(std::string_view name)const override{
        if(!valid_name(name))return std::unexpected(IconError{IconErrorCode::invalid_argument,"Invalid icon name"});
        const auto item=assets_.find(name);if(item==assets_.end())return std::unexpected(IconError{IconErrorCode::not_found,"Icon was not found"});
        return item->second;
    }
    [[nodiscard]] std::uint64_t revision()const noexcept override{return 1;}
    [[nodiscard]] std::size_t size()const noexcept{return assets_.size();}
};
[[nodiscard]] inline IconResult<std::shared_ptr<const IconCatalog>> builtin_icon_catalog(){
    static const auto value=[] {
        std::vector<IconDefinition> entries;entries.reserve(detail::builtin_icon_assets.size());
        for(const auto& item:detail::builtin_icon_assets)entries.push_back({std::string(item.name),std::string(item.svg)});
        return IconCatalog::create(std::move(entries));
    }();
    return value;
}
} // namespace lvui
