#pragma once
#include <LVUI/EventListener.h>
#include <LVUI/Text.h>
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
struct EventListenerCallbacks final {
    std::function<ii::core::Result<bool>(const ii::gui::WidgetInput&)> input;
    std::function<void(ii::gui::Rect)> resized;
};
class NativeEventListener final {
    struct State final {
        EventListener view;
        ii::gui::Widget root;
        EventListenerCallbacks callbacks;
        std::function<void(ii::gui::Rect)> layout;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t options_revision=0,callbacks_revision=0;
        bool live=false,retired=false,dispatching=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeEventListener(std::shared_ptr<State> state):state_(std::move(state)){}
    struct Guard final {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}};
    [[nodiscard]] static bool valid_input(const ii::gui::WidgetInput& value)noexcept {
        return unsigned(value.kind)<=unsigned(ii::gui::WidgetInputKind::commit_composition)&&unsigned(value.key)<=unsigned(ii::gui::WidgetKey::redo)&&
            unsigned(value.device)<=unsigned(ii::gui::PointerDevice::touch)&&std::isfinite(value.x)&&std::isfinite(value.y)&&
            std::isfinite(value.scroll_x)&&std::isfinite(value.scroll_y)&&std::isfinite(value.pressure)&&value.pressure>=0&&value.pressure<=1&&
            std::isfinite(value.tilt_x)&&std::abs(value.tilt_x)<=90&&std::isfinite(value.tilt_y)&&std::abs(value.tilt_y)<=90&&
            std::isfinite(value.rotation)&&std::isfinite(value.timestamp)&&value.timestamp>=0&&
            valid_accessibility_text(value.text)&&valid_accessibility_text(value.target);
    }
    [[nodiscard]] static ii::core::Result<bool> forward(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& value){
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Event listener has retired");
        if(state->dispatching)return ii::core::fail(ii::core::ErrorCode::busy,"Event listener is already dispatching");
        if(!valid_input(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener input");
        Guard guard(state->dispatching);
        try {auto callback=state->callbacks.input;return callback?callback(value):ii::core::Result<bool>{false};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Event listener callback failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeEventListener> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,EventListenerConfiguration value={},EventListenerCallbacks callbacks={}){
        if(!EventListener::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener extent");
        try {
            auto state=std::make_shared<State>();(void)state->view.set_configuration(value);state->callbacks=std::move(callbacks);
            auto root=builder.absolute_stack(parent,id,{value.width,value.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());std::weak_ptr<State> weak=state;
            if(auto installed=root->set_input_handler([weak](const ii::gui::WidgetInput& input)->ii::core::Result<bool>{
                if(auto state=weak.lock();state&&state->live)return forward(state,input);return false;
            });!installed)return std::unexpected(installed.error());
            state->layout=[weak](ii::gui::Rect bounds){if(auto state=weak.lock();state&&state->live){auto callback=state->callbacks.resized;if(callback)callback(bounds);}};
            if(auto installed=root->set_layout_handler(state->layout);!installed)return std::unexpected(installed.error());
            // A candidate can be measured before it becomes visible. Resetting
            // layout delivery on publication makes the first live size observable.
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired){
                state->live=true;(void)state->root.set_layout_handler(state->layout);
            }});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;state->live=false;state->callbacks={};
            }});!registered)return std::unexpected(registered.error());
            return NativeEventListener(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Event listener construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(EventListenerConfiguration value)const {
        if(!EventListener::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener extent");
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(state_->options_revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Event listener options revision exhausted");
        const auto revision=++state_->options_revision;
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        if(state_->options_revision!=revision)return {};
        if(auto resized=state_->root.set_extent({value.width,value.height});!resized)return resized;
        if(state_->options_revision!=revision)return {};
        (void)state_->view.set_configuration(value);return {};
    }
    [[nodiscard]] ii::core::Result<void> set_callbacks(EventListenerCallbacks value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(state_->callbacks_revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Event listener callbacks revision exhausted");
        const auto revision=++state_->callbacks_revision;
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        if(state_->callbacks_revision!=revision)return {};
        std::swap(state_->callbacks,value);return {};
    }
    [[nodiscard]] ii::core::Result<bool> dispatch(const ii::gui::WidgetInput& value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(state_->dispatching)return ii::core::fail(ii::core::ErrorCode::busy,"Event listener is already dispatching");
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());return forward(state_,value);
    }
};
} // namespace lvui::native
