#pragma once
#include <iiSDKCore/Core/Value/Value.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
// Applications supply pages and navigation targets. Neither interface knows
// about a particular host, window, widget tree, or page implementation.
class NavigationPage {
public:
    virtual ~NavigationPage()=default;
};
struct NavigationRequest final {
    std::string href;
    ii::core::Value params{ii::core::Value::Object{}};
    std::shared_ptr<const NavigationPage> target;
    bool replace=false;
};
class NavigationTarget {
public:
    virtual ~NavigationTarget()=default;
    [[nodiscard]] virtual ii::core::Result<bool> navigate(const NavigationRequest&)=0;
};
class NavigationContext final {
public:
    void set_target(const std::shared_ptr<NavigationTarget>& target)noexcept{owner_.reset();target_=target;}
    // A host can release only the target binding it published itself.
    void bind_target(const std::shared_ptr<NavigationTarget>& target,const std::shared_ptr<const void>& owner)noexcept{owner_=owner;target_=target;}
    void unbind_target(const std::shared_ptr<const void>& owner)noexcept{if(owner&&owner_.lock()==owner){owner_.reset();target_.reset();}}
    [[nodiscard]] std::shared_ptr<NavigationTarget> target()const noexcept{return target_.lock();}
private:
    std::weak_ptr<NavigationTarget> target_;
    std::weak_ptr<const void> owner_;
};

// A bounded request remains safe to copy and dispatch across replaceable
// navigation implementations. Values remain typed iiSDKCore data.
struct NavigationParameterStats final {std::size_t nodes=0,bytes=0;};
[[nodiscard]] inline std::optional<NavigationParameterStats> navigation_parameter_stats(const ii::core::Value& value)noexcept {
    constexpr std::size_t max_nodes=4096,max_depth=64,max_bytes=1024*1024;
    std::size_t nodes=0,bytes=0;
    const auto add=[&](std::size_t count){if(count>max_bytes-bytes)return false;bytes+=count;return true;};
    const auto visit=[&](const auto& self,const ii::core::Value& current,std::size_t depth)->bool{
        if(++nodes>max_nodes||depth>max_depth)return false;
        const auto& storage=current.storage();
        if(auto text=std::get_if<std::string>(&storage)){if(!add(text->size()))return false;}
        else if(auto data=std::get_if<ii::core::Bytes>(&storage)){if(!add(data->size()))return false;}
        else if(auto list=std::get_if<ii::core::Value::Array>(&storage)){
            if(list->size()>max_nodes-nodes)return false;
            for(const auto& child:*list)if(!self(self,child,depth+1))return false;
        }else if(auto object=std::get_if<ii::core::Value::Object>(&storage)){
            if(object->size()>max_nodes-nodes)return false;
            for(const auto& [key,child]:*object)if(!add(key.size())||!self(self,child,depth+1))return false;
        }
        return true;
    };
    if(!visit(visit,value,0))return std::nullopt;
    return NavigationParameterStats{nodes,bytes};
}
[[nodiscard]] inline bool valid_navigation_parameters(const ii::core::Value& value)noexcept{return navigation_parameter_stats(value).has_value();}
} // namespace lvui
