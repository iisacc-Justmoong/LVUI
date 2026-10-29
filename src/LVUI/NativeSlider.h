#pragma once
#include "Slider.h"
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
inline StyleColor slider_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
class NativeSlider final {
    struct State final {
        Slider view;
        ii::gui::Widget root;
        std::function<void(double)> moved;
        const std::thread::id owner=std::this_thread::get_id();
        double last_input_value=.5;
        std::uint64_t revision=0;
        bool dragging=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeSlider(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action(const SliderConfiguration& value){
        ii::gui::ActionOptions options;options.accessibility_name=value.accessibility_name;
        options.enabled=value.enabled;options.focus_ring=false;options.width=value.width;options.height=value.height;return options;
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const SliderConfiguration& value){
        ii::gui::AccessibilitySemantics result;result.role=ii::gui::AccessibilityRole::slider;
        result.name=value.accessibility_name;result.range=ii::gui::AccessibilityRange{value.minimum,value.maximum,value.value};return result;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& paint_state){
        auto geometry=state->view.geometry(paint_state.size.width,paint_state.size.height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider extent");
        const auto rectangle=[](SliderRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
        const auto linear=[&](StyleColor value){const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha*(paint_state.enabled?1:.5))};};
        const auto& style=state->view.configuration();auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        const auto track=rectangle(geometry->track),fill=rectangle(geometry->fill);
        if(track.width>0){auto color=linear(style.palette.track);drawing->panels.push_back({track,color,color,std::min(track.width,track.height)/2});}
        if(fill.width>0){auto color=linear(style.palette.fill);drawing->panels.push_back({fill,color,color,std::min(fill.width,fill.height)/2});}
        auto thumb=linear(style.palette.thumb);drawing->panels.push_back({rectangle(geometry->thumb),thumb,thumb,float(style.thumb_diameter/2)});
        if(paint_state.focused){auto focus=linear(style.palette.focus);drawing->panels.push_back({rectangle(geometry->thumb),focus,focus,float(style.thumb_diameter/2),float(std::min(1.5,style.thumb_diameter/2))});}
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> update_value(const std::shared_ptr<State>& state,Slider next){
        if(next.value()==state->view.value())return {};
        if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI Slider revision exhausted");
        const auto previous=state->view;const auto revision=++state->revision;state->view=std::move(next);
        try {
            if(auto changed=state->root.set_accessibility_semantics(semantics(state->view.configuration()));!changed){
                if(state->revision==revision)state->view=previous;return changed;
            }
            if(state->revision!=revision)return {};
            if(auto painted=state->root.request_repaint();!painted)return painted;
            if(state->revision==revision&&state->moved){auto callback=state->moved;callback(state->view.value());}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider input callback failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;using Key=ii::gui::WidgetKey;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        try {
            if(event.kind==Kind::focus_lost){state->dragging=false;return false;}
            if(!state->view.configuration().enabled)return false;
            Slider next=state->view;bool handled=false;
            if(event.kind==Kind::pointer_down||(state->dragging&&(event.kind==Kind::pointer_move||event.kind==Kind::pointer_up))){
                const auto revision=state->revision;
                // Explicit focus also propagates through an embedded content host,
                // allowing later keyboard events to reach this owned control.
                if(event.kind==Kind::pointer_down){if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());}
                if(state->revision!=revision)return true;
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                if(state->revision!=revision)return true;
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Slider pointer input requires local coordinates");
                if(event.kind==Kind::pointer_down)state->dragging=true;
                if(event.kind==Kind::pointer_up)state->dragging=false;
                handled=next.set_from_position(event.local_position->x,snapshot->bounds.width);
            }else if(event.kind==Kind::increment)handled=next.nudge(1);
            else if(event.kind==Kind::decrement)handled=next.nudge(-1);
            else if(event.kind==Kind::key_down){
                if(event.key==Key::left||event.key==Key::down)handled=next.nudge(-1);
                else if(event.key==Key::right||event.key==Key::up)handled=next.nudge(1);
                else if(event.key==Key::home)handled=next.set_value(next.configuration().minimum);
                else if(event.key==Key::end)handled=next.set_value(next.configuration().maximum);
            }
            if(!handled)return false;
            if(auto applied=update_value(state,std::move(next));!applied)return std::unexpected(applied.error());
            return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider input failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeSlider> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const SliderConfiguration& configuration,std::function<void(double)> moved={}){
        try {
            auto state=std::make_shared<State>();
            if(!state->view.set_configuration(configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider configuration");
            state->last_input_value=configuration.value;state->moved=std::move(moved);
            auto root=builder.action_surface(parent,id,action(state->view.configuration()));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto accessible=root->set_accessibility_semantics(semantics(state->view.configuration()));!accessible)return std::unexpected(accessible.error());
            if(auto installed=root->set_paint_handler([state](const auto& value){return paint(state,value);});!installed)return std::unexpected(installed.error());
            if(auto installed=root->set_input_handler([state](const auto& event){return input(state,event);});!installed)return std::unexpected(installed.error());
            return NativeSlider(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<SliderConfiguration> options()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        if(auto valid=state_->root.action_options();!valid)return std::unexpected(valid.error());
        try{return state_->view.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(SliderConfiguration value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        if(!Slider::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider configuration");
        try {
            if(auto valid=state_->root.action_options();!valid)return std::unexpected(valid.error());
            if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI Slider revision exhausted");
            const auto incoming=value.value;if(incoming==state_->last_input_value)value.value=state_->view.value();
            auto next=state_->view;if(!next.set_configuration(std::move(value)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider configuration");
            const auto previous=state_->view;const auto previous_input=state_->last_input_value;const auto revision=++state_->revision;
            state_->view=std::move(next);state_->last_input_value=incoming;
            if(!state_->view.configuration().enabled)state_->dragging=false;
            if(auto changed=state_->root.set_action_options(action(state_->view.configuration()));!changed){
                if(state_->revision==revision){state_->view=previous;state_->last_input_value=previous_input;}return changed;
            }
            if(state_->revision!=revision)return {};
            if(auto accessible=state_->root.set_accessibility_semantics(semantics(state_->view.configuration()));!accessible){
                if(state_->revision==revision){state_->view=previous;state_->last_input_value=previous_input;}return accessible;
            }
            if(state_->revision!=revision)return {};
            return state_->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider update failed",std::current_exception());}
    }
};
} // namespace lvui::native
