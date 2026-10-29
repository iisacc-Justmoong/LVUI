#pragma once
#include "NativeTooltipTarget.h"
#include <memory>
#include <string_view>
#include <utility>

namespace lvui::native {
// LVUI owns the model, geometry, content and scheduling policy. The builder
// supplies only generic widget services and the application's Core event loop.
class NativeTooltip final {
    std::shared_ptr<NativeTooltipPresentation> presentation_;
    TooltipController controller_;
    NativeTooltipScheduler scheduler_;
    NativeTooltipTarget target_;
    NativeTooltip(std::shared_ptr<NativeTooltipPresentation> presentation,TooltipController controller,
            NativeTooltipScheduler scheduler,NativeTooltipTarget target)
        :presentation_(std::move(presentation)),controller_(std::move(controller)),
         scheduler_(std::move(scheduler)),target_(std::move(target)){}
public:
    [[nodiscard]] static ii::core::Result<NativeTooltip> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent,std::string_view id,TooltipConfiguration configuration={},
            TooltipPresentationStyle style={},TooltipTargetOptions options={},TooltipCallbacks callbacks={}){
        if(!Tooltip::validate(configuration)||configuration.content_padding!=style.content_padding)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip content padding and configuration must agree");
        auto loop=builder.event_loop();if(!loop)return std::unexpected(loop.error());
        auto scheduler=NativeTooltipScheduler::create(loop->get().sender());if(!scheduler)return std::unexpected(scheduler.error());
        auto presentation=NativeTooltipPresentation::create(builder,std::move(parent),id,std::move(style),scheduler->weak_schedule());
        if(!presentation)return std::unexpected(presentation.error());
        auto controller=TooltipController::create(*presentation,std::move(configuration),std::move(callbacks));
        if(!controller)return std::unexpected(controller.error());
        auto target=NativeTooltipTarget::create(builder,*presentation,*controller,*scheduler,std::move(options));
        if(!target)return std::unexpected(target.error());
        if(auto wake=scheduler->set_wake(target->weak_tick());!wake)return std::unexpected(wake.error());
        return NativeTooltip(std::move(*presentation),std::move(*controller),std::move(*scheduler),std::move(*target));
    }
    [[nodiscard]] ii::gui::Widget root()const{return presentation_->root();}
    [[nodiscard]] ii::gui::Widget content_root()const{return presentation_->content_root();}
    [[nodiscard]] ii::core::Result<void> set_target(ii::gui::Widget target)const{return target_.set_target(std::move(target));}
    [[nodiscard]] ii::core::Result<void> refresh()const{return target_.refresh();}
    [[nodiscard]] ii::core::Result<void> open()const{return target_.open();}
    [[nodiscard]] ii::core::Result<void> close()const{return target_.close();}
    [[nodiscard]] ii::core::Result<TooltipSnapshot> snapshot()const {
        auto observing=target_.status();if(!observing)return std::unexpected(observing.error());
        return controller_.snapshot();
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> last_failure()const {
        auto observed=target_.last_failure();if(!observed||*observed)return observed;
        return scheduler_.last_failure();
    }
};
} // namespace lvui::native
