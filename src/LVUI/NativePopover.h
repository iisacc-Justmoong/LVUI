#pragma once
#include "Popover.h"
#include <iiGUI/NativeWidgets.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
[[nodiscard]] inline StyleColor popover_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct NativePopoverCallbacks final {std::function<void()> opened,closed,canceled;};

class NativePopover final {
    struct State final {
        Popover model;
        ii::gui::Widget root;
        NativePopoverCallbacks callbacks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false;
    };
    std::shared_ptr<State> state_;
    explicit NativePopover(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Popover requires its owner thread");
        if(auto current=state->root.snapshot();!current)return std::unexpected(current.error());return {};
    }
    [[nodiscard]] static ii::gui::PopoverOptions host_options(const PopoverConfiguration& value){
        ii::gui::PopoverOptions result;
        result.width=value.width;result.height=value.height;result.padding=value.padding;result.margins=value.margins;
        result.corner_radius=value.corner_radius;result.surface_opacity=value.surface_opacity;
        if(value.surface_color){const auto c=*value.surface_color;result.surface_color=ii::gui::WidgetColor{c.red,c.green,c.blue,c.alpha};}
        result.modal=value.modal;result.dim=value.dim;result.focus_on_open=value.focus_on_open;result.clip_content=value.clip_content;
        result.dismiss_on_escape=value.dismiss_on_escape;result.dismiss_on_outside_press=value.dismiss_on_outside_press;
        result.dismiss_on_context_request=value.dismiss_on_context_request;result.accessibility_name=value.accessibility_name;
        return result;
    }
public:
    [[nodiscard]] static ii::core::Result<NativePopover> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,PopoverConfiguration value={},NativePopoverCallbacks callbacks={}){
        if(!Popover::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Popover configuration");
        try{
            auto state=std::make_shared<State>();(void)state->model.set_configuration(value);state->callbacks=std::move(callbacks);
            std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member,bool open){return [weak,member,open]{if(auto value=weak.lock();value&&value->live){
                auto current=value->root.snapshot();if(!current||current->opened!=open)return;auto callback=value->callbacks.*member;if(callback)callback();}};};
            auto popup=builder.popover(parent,id,host_options(value),{.opened=event(&NativePopoverCallbacks::opened,true),
                .closed=event(&NativePopoverCallbacks::closed,false),.canceled=event(&NativePopoverCallbacks::canceled,false)});
            if(!popup)return std::unexpected(popup.error());state->root=*popup;
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto value=weak.lock())value->live=false;});!retired)return std::unexpected(retired.error());
            return NativePopover(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<PopoverConfiguration> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());
        try{return state_->model.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(PopoverConfiguration value)const{
        if(auto valid=check(state_);!valid)return valid;
        if(!Popover::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Popover configuration");
        try{
            if(value==state_->model.configuration())return {};
            if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Popover revision exhausted");
            auto host=host_options(value);auto previous=state_->model.configuration();const auto revision=++state_->revision;(void)state_->model.set_configuration(value);
            if(auto updated=state_->root.set_popover_options(std::move(host));!updated){if(state_->revision==revision)(void)state_->model.set_configuration(std::move(previous));return updated;}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Popover update failed",std::current_exception());}
    }
};
} // namespace lvui::native
