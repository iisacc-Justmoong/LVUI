#pragma once
#include "NativeTooltipPresentation.h"
#include "NativeTooltipScheduler.h"
#include <iiGUI/WidgetInteraction.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace lvui::native {
struct TooltipTargetOptions final {
    std::optional<TooltipRect> available;
    std::optional<TooltipPoint> anchor;
};

// Observation never replaces the target's input handlers. The controller owns
// the state policy, while iiGUI supplies weak, generation-specific metadata.
class NativeTooltipTarget final {
    struct State final {
        std::shared_ptr<NativeTooltipPresentation> presentation;
        TooltipController controller;
        NativeTooltipScheduler scheduler;
        TooltipTargetOptions options;
        ii::gui::Widget target;
        std::shared_ptr<ii::gui::WidgetInteractionPort> target_port,popup_port;
        std::optional<ii::gui::WidgetInteraction> target_info,popup_info;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::atomic_bool foreign_delivery=false;
        std::uint64_t target_revision=0;
        bool live=false,retired=false,processing=false,pending=false;
        State(std::shared_ptr<NativeTooltipPresentation> p,TooltipController c,NativeTooltipScheduler s,TooltipTargetOptions o)
            :presentation(std::move(p)),controller(std::move(c)),scheduler(std::move(s)),options(std::move(o)){}
    };
    std::shared_ptr<State> state_;
    explicit NativeTooltipTarget(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool valid(const TooltipTargetOptions& value)noexcept {
        const auto coordinate=[](double n){return std::isfinite(n)&&std::abs(n)<=1000000000.;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        return (!value.anchor||(coordinate(value.anchor->x)&&coordinate(value.anchor->y)))&&
            (!value.available||(coordinate(value.available->x)&&coordinate(value.available->y)&&
                metric(value.available->width)&&metric(value.available->height)));
    }
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tooltip target requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip target has retired");
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> drive(const std::shared_ptr<State>& state){
        auto now=state->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        TooltipInteraction input;input.target_present=!state->target.is_null();
        input.bubble_hovered=state->popup_info&&state->popup_info->alive&&state->popup_info->hovered;
        if(state->target_info){
            const auto& target=*state->target_info;
            input.target_available=target.alive&&target.visible&&target.enabled&&!target.input_blocked;
            input.target_engaged=target.hovered||target.pressed||target.focused;
            if(input.target_available){
                const TooltipPoint anchor{target.bounds.x+(state->options.anchor?state->options.anchor->x:target.bounds.width/2),
                    target.bounds.y+(state->options.anchor?state->options.anchor->y:target.bounds.height/2)};
                const auto& clip=target.clip;
                if(anchor.x<clip.x||anchor.y<clip.y||anchor.x>clip.x+clip.width||anchor.y>clip.y+clip.height)
                    input.target_available=false;
                else {
                    const TooltipRect available=state->options.available.value_or(TooltipRect{0,0,target.surface.width,target.surface.height});
                    auto config=state->controller.configuration();if(!config)return std::unexpected(config.error());
                    const auto max_width=std::clamp(config->maximum_width-2*config->content_padding,1.,8192.);
                    auto measured=state->presentation->content_root().measure(max_width);if(!measured)return std::unexpected(measured.error());
                    const TooltipGeometryInput geometry{available,anchor,{measured->width,measured->height}};
                    auto positioned=state->controller.set_geometry(geometry);
                    if(!positioned){
                        if(positioned.error().code==ii::core::ErrorCode::invalid_argument)input.target_available=false;
                        else return positioned;
                    }
                }
            }
        }
        return state->controller.advance(*now,input);
    }
    static void enqueue(const std::shared_ptr<State>& state)noexcept {
        if(!state->live||state->retired)return;state->pending=true;if(state->processing)return;
        state->processing=true;struct Reset final{State& state;~Reset(){state.processing=false;}} reset{*state};
        try {
            for(unsigned pass=0;state->pending&&pass<8;++pass){
                state->pending=false;auto result=drive(state);
                if(!result){state->failure=result.error();return;}state->failure.reset();
                if(state->retired||!state->live)return;
            }
            if(state->pending)state->failure=ii::core::Error{ii::core::ErrorCode::busy,"Tooltip observation did not settle"};
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Tooltip observation failed",std::current_exception()};}catch(...) {}}
    }
    template<class Function>
    [[nodiscard]] static ii::core::Result<void> coordinated(const std::shared_ptr<State>& state,Function function){
        if(auto valid=ready(state);!valid)return valid;
        if(!state->live||state->processing)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip observation is not ready");
        state->processing=true;ii::core::Result<void> result;
        try{result=function();}catch(...){result=ii::core::fail(ii::core::ErrorCode::failed,"Tooltip operation failed",std::current_exception());}
        state->processing=false;
        const bool deliver=state->pending&&!state->retired;if(deliver)enqueue(state);
        if(result&&deliver&&state->failure)return std::unexpected(*state->failure);return result;
    }
public:
    [[nodiscard]] static ii::core::Result<NativeTooltipTarget> create(ii::gui::WidgetBuilder& builder,
            std::shared_ptr<NativeTooltipPresentation> presentation,TooltipController controller,
            NativeTooltipScheduler scheduler,TooltipTargetOptions options={}){
        if(!presentation||!valid(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid tooltip target options");
        auto matching=controller.uses_port(presentation.get());if(!matching)return std::unexpected(matching.error());
        if(!*matching)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip controller uses another presentation port");
        try {
            auto state=std::make_shared<State>(std::move(presentation),std::move(controller),std::move(scheduler),std::move(options));
            std::weak_ptr<State> weak=state;
            state->popup_port=std::make_shared<ii::gui::WidgetInteractionPort>([weak](const ii::gui::WidgetInteraction& value){
                if(auto state=weak.lock()){
                    if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                    if(!state->retired){state->popup_info=value;enqueue(state);}
                }
            });
            if(auto observed=state->presentation->root().observe_interaction(state->popup_port);!observed)return std::unexpected(observed.error());
            if(auto measured=state->presentation->content_root().set_measure_handler([weak](const ii::gui::WidgetMeasurement&){
                if(auto state=weak.lock()){
                    if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                    if(!state->retired)enqueue(state);
                }
            });!measured)return std::unexpected(measured.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired){
                state->live=true;if(!state->target.is_null()){
                    auto current=state->target.interaction();
                    if(!current)state->failure=current.error();else{state->target_info=*current;enqueue(state);}
                }
            }});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;state->live=false;state->target_port.reset();state->popup_port.reset();
            }});!registered)return std::unexpected(registered.error());
            return NativeTooltipTarget(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip observation construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_target(ii::gui::Widget target)const {
        if(auto valid=ready(state_);!valid)return valid;
        if(state_->processing)return ii::core::fail(ii::core::ErrorCode::busy,"Tooltip observation is updating");
        if(state_->target_revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tooltip target revision exhausted");
        try {
            if(target.is_null()){
                if(state_->live&&!state_->target.is_null()){
                    auto now=state_->scheduler.elapsed();if(!now)return std::unexpected(now.error());
                    if(auto closed=coordinated(state_,[&]{return state_->controller.close(*now);});!closed)return closed;
                }
                ++state_->target_revision;state_->target={};state_->target_info.reset();state_->target_port.reset();state_->failure.reset();return {};
            }
            auto target_place=target.placement();if(!target_place)return std::unexpected(target_place.error());
            auto popup_place=state_->presentation->root().placement();if(!popup_place)return std::unexpected(popup_place.error());
            auto target_domain=target_place->domain.lock(),popup_domain=popup_place->domain.lock();
            if(!target_domain||target_domain!=popup_domain)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tooltip target belongs to another widget domain");
            const auto revision=state_->target_revision+1;std::weak_ptr<State> weak=state_;
            auto port=std::make_shared<ii::gui::WidgetInteractionPort>([weak,revision](const ii::gui::WidgetInteraction& value){
                if(auto state=weak.lock()){
                    if(state->owner!=std::this_thread::get_id()){state->foreign_delivery.store(true,std::memory_order_relaxed);return;}
                    if(!state->retired&&state->target_revision==revision){state->target_info=value;enqueue(state);}
                }
            });
            if(auto observed=target.observe_interaction(port);!observed)return observed;
            state_->target_revision=revision;state_->target=std::move(target);state_->target_info.reset();state_->target_port=std::move(port);state_->failure.reset();
            if(state_->live){auto current=state_->target.interaction();if(!current)return std::unexpected(current.error());
                state_->target_info=*current;enqueue(state_);
                if(state_->failure)return std::unexpected(*state_->failure);}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tooltip target update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> last_failure()const {
        if(auto valid=ready(state_);!valid)return std::unexpected(valid.error());
        if(state_->foreign_delivery.exchange(false,std::memory_order_relaxed))
            return std::optional{ii::core::Error{ii::core::ErrorCode::wrong_executor,"Tooltip observation arrived on another thread"}};
        return state_->failure;
    }
    [[nodiscard]] ii::core::Result<void> status()const{return ready(state_);}
    [[nodiscard]] ii::core::Result<void> refresh()const {
        if(auto valid=ready(state_);!valid)return valid;
        // Binding evaluation may precede publication, and opened/closed handlers
        // may change content while an observation is being delivered.
        if(!state_->live)return {};
        if(state_->processing){state_->pending=true;return {};}
        enqueue(state_);if(state_->failure)return std::unexpected(*state_->failure);return {};
    }
    [[nodiscard]] ii::core::Result<void> open()const {
        if(auto valid=ready(state_);!valid)return valid;auto now=state_->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        return coordinated(state_,[&]{return state_->controller.open(*now);});
    }
    [[nodiscard]] ii::core::Result<void> close()const {
        if(auto valid=ready(state_);!valid)return valid;auto now=state_->scheduler.elapsed();if(!now)return std::unexpected(now.error());
        return coordinated(state_,[&]{return state_->controller.close(*now);});
    }
    [[nodiscard]] NativeTooltipScheduler::Wake weak_tick()const {
        std::weak_ptr<State> weak=state_;
        return [weak](std::int64_t now)->ii::core::Result<void>{
            auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tooltip observer expired");
            return coordinated(state,[&]{return state->controller.tick(now);});
        };
    }
};
} // namespace lvui::native
