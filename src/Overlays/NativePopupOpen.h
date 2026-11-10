#pragma once

#include <LVUI/NativeBindings.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace lvui::native {

// The source can settle during construction; opening is admitted only after
// the containing generation has published. The binding graph owns diagnostics
// and retries failed changes without owning the widget generation itself.
template<class Builder, class Popup, class Getter>
[[nodiscard]] ii::core::Result<BindingId> bind_popup_open(
        Builder& builder,const std::shared_ptr<Bindings>& bindings,
        Popup target,Getter getter){
    static_assert(std::is_same_v<std::invoke_result_t<Getter&,ReadContext&>,bool>);
    if(!bindings||!bindings->available())return ii::core::fail(ii::core::ErrorCode::closed,"Popup binding owner retired");
    try{
        auto published=std::make_shared<ii::core::Observable<bool>>(false);
        auto binding=bindings->bind(
            [published,getter=std::move(getter)](ReadContext& context)mutable{
                return std::pair<bool,bool>{context.read(*published),std::invoke(getter,context)};
            },
            [target](const std::pair<bool,bool>& value)->ii::core::Result<void>{
                if(!value.first)return {};
                auto snapshot=target.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                if(snapshot->opened==value.second)return {};
                return value.second?target.open_at(0,0):target.close_popup();
            });
        if(!binding)return std::unexpected(binding.error());
        if(auto hook=builder.on_publish([weak=std::weak_ptr(published)]{if(auto value=weak.lock())value->set(true);});!hook){
            (void)bindings->remove(*binding);return std::unexpected(hook.error());
        }
        return *binding;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popup open binding failed",std::current_exception());}
}

} // namespace lvui::native
