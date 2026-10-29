#pragma once
#include "SegmentedControl.h"
#include "NativeStylePort.h"
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline SegmentedShape segmented_shape(SegmentedShape value)noexcept{return value;}
[[nodiscard]] inline StyleColor segmented_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
// Sinks adapt independently owned child controls; a closed child is removed.
using SegmentedToneSink=BorderlessTonePort;
using SegmentedRegistrationPort=std::function<ii::core::Result<void>(SegmentedToneSink)>;
class NativeSegmentedControl final {
    struct State final {
        struct Registration final {std::uint64_t id;SegmentedToneSink sink;bool applied;};
        SegmentedControl model;
        ii::gui::Widget root;
        std::vector<Registration> sinks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_registration=0;
    };
    std::shared_ptr<State> state_;
    explicit NativeSegmentedControl(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::StackOptions layout(const SegmentedControlConfiguration& c){
        ii::gui::StackOptions result;result.spacing=c.spacing;result.alignment=ii::gui::StackAlignment::center;
        result.insets=ii::gui::Insets{c.horizontal_padding,c.vertical_padding,c.horizontal_padding,c.vertical_padding};
        result.intrinsic_width=true;return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<void> deliver(const SegmentedToneSink& sink,bool value){
        try{return sink.apply(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child style failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        try{
            auto geometry=state->model.geometry(input.size.width,input.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented surface extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();const auto& value=state->model.configuration();
            if(geometry->width>0&&geometry->height>0){
                const ii::gui::Rect bounds{0,0,float(geometry->width),float(geometry->height)};
                if(value.background.alpha>0){const auto ink=linear(value.background);result->panels.push_back({bounds,ink,ink,float(geometry->radius)});}
                if(value.border.alpha>0&&geometry->border_width>0){const auto ink=linear(value.border);result->panels.push_back({bounds,ink,ink,float(geometry->radius),float(geometry->border_width)});}
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented surface paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeSegmentedControl> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,SegmentedControlConfiguration config={}){
        try{
            auto state=std::make_shared<State>();if(!state->model.set_configuration(std::move(config)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented control configuration");
            auto root=builder.hstack(parent,id,layout(state->model.configuration()));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_enabled(state->model.configuration().enabled);!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            return NativeSegmentedControl(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented control creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] SegmentedRegistrationPort weak_tone_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](SegmentedToneSink sink)->ii::core::Result<void>{
            auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
            return NativeSegmentedControl(std::move(state)).register_tone_sink(std::move(sink));
        };
    }
    [[nodiscard]] ii::core::Result<SegmentedControlConfiguration> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        return state_->model.configuration();
    }
    [[nodiscard]] ii::core::Result<std::size_t> prune_retired_tone_sinks()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        try{
            const auto entries=state_->sinks;std::vector<std::uint64_t> retired;
            for(const auto& entry:entries)if(entry.sink.retired&&entry.sink.retired())retired.push_back(entry.id);
            if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
            const auto before=state_->sinks.size();
            std::erase_if(state_->sinks,[&](const auto& entry){return std::ranges::find(retired,entry.id)!=retired.end();});
            return before-state_->sinks.size();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child cleanup failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> register_tone_sink(std::function<ii::core::Result<void>(bool)> sink)const{
        return register_tone_sink(SegmentedToneSink{.apply=std::move(sink)});
    }
    [[nodiscard]] ii::core::Result<void> register_tone_sink(SegmentedToneSink sink)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state_->sinks.size()>=4096){auto pruned=prune_retired_tone_sinks();if(!pruned)return std::unexpected(pruned.error());}
        if(!sink.apply||state_->sinks.size()>=4096||state_->next_registration==std::numeric_limits<std::uint64_t>::max())
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented child registration");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        try{
            for(unsigned attempt=0;attempt<8;++attempt){
                const auto revision=state_->revision;auto result=deliver(sink,state_->model.configuration().force_borderless_tone);
                if(!result)return result;
                if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
                if(state_->revision==revision){state_->sinks.push_back({++state_->next_registration,std::move(sink),state_->model.configuration().force_borderless_tone});return {};}
            }
            return ii::core::fail(ii::core::ErrorCode::busy,"Segmented child style did not settle");
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child registration failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(SegmentedControlConfiguration config)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(!SegmentedControl::validate(config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented control configuration");
        try{
            if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
            if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Segmented control revision exhausted");
            const auto previous=state_->model.configuration();const auto revision=++state_->revision;
            (void)state_->model.set_configuration(config);
            const auto restore=[&]{if(state_->revision==revision){(void)state_->model.set_configuration(previous);
                (void)state_->root.set_stack_options(layout(previous));if(state_->revision==revision)(void)state_->root.set_enabled(previous.enabled);}};
            if(auto r=state_->root.set_stack_options(layout(config));!r){restore();return r;}
            if(state_->revision!=revision)return {};
            if(auto r=state_->root.set_enabled(config.enabled);!r){restore();return r;}
            if(state_->revision!=revision)return {};
            {
                const auto sinks=state_->sinks;std::vector<State::Registration> changed;std::vector<std::uint64_t> closed;
                const auto mark=[&](std::uint64_t id,bool applied){
                    auto entry=std::ranges::find_if(state_->sinks,[&](const auto& value){return value.id==id;});
                    if(entry!=state_->sinks.end())entry->applied=applied;
                };
                for(const auto& entry:sinks){
                    if(entry.applied==config.force_borderless_tone)continue;
                    auto r=deliver(entry.sink,config.force_borderless_tone);
                    if(state_->revision!=revision)return {};
                    if(!r&&r.error().code==ii::core::ErrorCode::closed){closed.push_back(entry.id);continue;}
                    if(!r){for(auto item=changed.rbegin();item!=changed.rend()&&state_->revision==revision;++item)
                            if(auto restored=deliver(item->sink,item->applied);restored&&state_->revision==revision)mark(item->id,item->applied);
                        restore();return r;}
                    changed.push_back(entry);mark(entry.id,config.force_borderless_tone);
                }
                std::erase_if(state_->sinks,[&](const auto& entry){return std::ranges::find(closed,entry.id)!=closed.end();});
            }
            if(state_->revision!=revision)return {};
            return state_->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented control update failed",std::current_exception());}
    }
};
} // namespace lvui::native
