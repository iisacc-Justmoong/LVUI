#pragma once
#include "NativeTab.h"
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {
using TabRegistrationPort=std::function<ii::core::Result<void>(TabGroupPort)>;
class NativeTabBar final {
    struct State final {
        struct Entry final {std::uint64_t id;std::string key;TabGroupPort port;TabPresentation applied;};
        TabBar model;
        ii::gui::Widget root;
        std::vector<Entry> entries;
        std::optional<double> available_width;
        std::function<void(int)> changed;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_id=0;
        bool finalized=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeTabBar(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::StackOptions layout(const TabBarConfiguration& value){
        return {.spacing=value.spacing,.alignment=ii::gui::StackAlignment::center};
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const TabBarConfiguration& value){
        return {.role=ii::gui::AccessibilityRole::tab_group,.name=value.accessibility_name};
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"TabBar requires its owner thread");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static ii::core::Result<void> present(const TabGroupPort& port,TabPresentation value){
        try{return port.present(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child presentation failed",std::current_exception());}
    }
    // Retain selection by registration identity when earlier children retire.
    [[nodiscard]] static ii::core::Result<void> collect(const std::shared_ptr<State>& state){
        try{
        auto current=state->model.configuration().current_index;std::optional<std::uint64_t> selected;
        if(current>=0&&std::size_t(current)<state->entries.size())selected=state->entries[std::size_t(current)].id;
        std::erase_if(state->entries,[](const auto& entry){return entry.port.retired&&entry.port.retired();});
        if(selected){auto item=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==*selected;});
            auto config=state->model.configuration();config.current_index=item==state->entries.end()?-1:int(item-state->entries.begin());
            (void)state->model.set_configuration(config);}
        std::vector<bool> enabled;enabled.reserve(state->entries.size());
        for(const auto& entry:state->entries){auto value=entry.port.availability();if(!value)return std::unexpected(value.error());enabled.push_back(value->enabled&&value->visible);}
        if(!state->model.set_items(std::move(enabled)))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Too many tabs");return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child inspection failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> sync(const std::shared_ptr<State>& state){
        try{
        if(auto valid=check(state);!valid)return valid;
        const auto previous_index=state->model.configuration().current_index;
        if(auto collected=collect(state);!collected)return collected;
        auto config=state->model.configuration();
        if(state->finalized&&config.auto_select&&previous_index>=0&&
           (config.current_index<0||!state->model.enabled(std::size_t(config.current_index)))){
            auto first=state->model.focus_target(std::nullopt,TabFocusMove::first);config.current_index=first?int(*first):-1;
            (void)state->model.set_configuration(config);
        }
        const auto observed=state->revision;
        std::optional<double> width;
        if(state->available_width&&*state->available_width>0&&state->model.count()){
            std::size_t visible=0;double minimum=config.minimum_tab_width;
            for(const auto& entry:state->entries){auto value=entry.port.availability();if(!value)return std::unexpected(value.error());
                if(value->visible){++visible;auto required=entry.port.minimum_width();if(!required)return std::unexpected(required.error());minimum=std::max(minimum,*required);}}
            if(visible){const auto gaps=double(visible-1)*config.spacing;
                width=std::min(8192.,std::max(minimum,std::max(0.,*state->available_width-gaps)/double(visible)));}
        }
        const auto entries=state->entries;std::vector<std::size_t> changed;changed.reserve(entries.size());
        const auto mark=[&](std::uint64_t id,TabPresentation value){
            auto found=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
            if(found!=state->entries.end())found->applied=value;
        };
        for(std::size_t index=0;index<entries.size();++index){
            const auto& entry=entries[index];TabPresentation desired{.selected=config.auto_select?std::optional<bool>{config.current_index==int(index)}:std::nullopt,.width=width};
            if(entry.applied==desired)continue;
            auto result=present(entry.port,desired);if(state->revision!=observed)return {};
            if(!result){
                for(auto previous=changed.rbegin();previous!=changed.rend()&&state->revision==observed;++previous)
                    if(auto restored=present(entries[*previous].port,entries[*previous].applied);restored&&state->revision==observed)mark(entries[*previous].id,entries[*previous].applied);
                return result;
            }
            changed.push_back(index);mark(entry.id,desired);
        }
        if(state->finalized&&config.auto_select&&config.current_index!=previous_index&&state->revision==observed){
            auto callback=state->changed;if(callback)callback(config.current_index);
        }
        return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar child synchronization failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,TabBarConfiguration next){
        if(auto valid=check(state);!valid)return valid;
        if(!TabBar::validate(next))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI TabBar configuration");
        try{
            if(auto collected=collect(state);!collected)return collected;
            if(state->finalized&&state->entries.empty()&&next.current_index==0)next.current_index=-1;
            if(state->finalized&&next.current_index>=0){
                if(std::size_t(next.current_index)>=state->entries.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar selection is outside its children");
                auto selected=state->entries[std::size_t(next.current_index)].port.availability();
                if(!selected)return std::unexpected(selected.error());
                if(!selected->enabled||!selected->visible)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar selection must be enabled and visible");
            }
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar revision exhausted");
            const auto previous=state->model.configuration();const auto revision=++state->revision;(void)state->model.set_configuration(next);
            const auto restore=[&]{if(state->revision!=revision)return;(void)state->model.set_configuration(previous);
                (void)state->root.set_stack_options(layout(previous));if(state->revision!=revision)return;
                (void)state->root.set_accessibility_semantics(semantics(previous));if(state->revision==revision)(void)sync(state);};
            if(auto changed=state->root.set_stack_options(layout(next));!changed){restore();return changed;}
            if(state->revision!=revision)return {};
            if(auto changed=state->root.set_accessibility_semantics(semantics(next));!changed){restore();return changed;}
            if(state->revision!=revision)return {};
            if(auto result=sync(state);!result){restore();return result;}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state,std::uint64_t id){
        if(auto valid=check(state);!valid)return valid;
        try{
            if(auto collected=collect(state);!collected)return collected;
            const auto found=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
            if(found==state->entries.end())return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
            const auto index=std::size_t(found-state->entries.begin());auto next=state->model;
            const auto activation=next.activate(index);
            if(activation==TabActivation::rejected)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Tab is disabled or hidden");
            if(activation==TabActivation::selection_changed){
                if(auto updated=apply(state,next.configuration());!updated)return updated;
                if(state->model.configuration().auto_select&&state->model.configuration().current_index==int(index)){
                    auto callback=state->changed;if(callback)callback(int(index));
                }
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar activation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        try{
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(event.kind!=WidgetInputKind::key_down)return false;
        TabFocusMove move;
        switch(event.key){case WidgetKey::left:move=TabFocusMove::previous;break;case WidgetKey::right:move=TabFocusMove::next;break;
            case WidgetKey::home:move=TabFocusMove::first;break;case WidgetKey::end:move=TabFocusMove::last;break;default:return false;}
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        if(auto collected=collect(state);!collected)return std::unexpected(collected.error());
        std::optional<std::size_t> focused;
        for(std::size_t i=0;i<state->entries.size();++i){auto value=state->entries[i].port.snapshot();if(!value)return std::unexpected(value.error());if(value->focused){focused=i;break;}}
        if(!focused)return false;
        auto target=state->model.focus_target(focused,move);if(!target)return true;
        auto result=state->entries[*target].port.focus();if(!result)return std::unexpected(result.error());return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar keyboard handling failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeTabBar> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,TabBarConfiguration config={},std::function<void(int)> changed={}){
        if(!TabBar::validate(config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI TabBar configuration");
        try{
            auto state=std::make_shared<State>();(void)state->model.set_configuration(config);state->changed=std::move(changed);
            auto root=builder.hstack(parent,id,layout(config));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_extent({0,40});!r)return std::unexpected(r.error());
            if(auto r=root->set_accessibility_semantics(semantics(config));!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([state](const auto& event){return input(state,event);});!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([state](ii::gui::Rect bounds){
                if(state->available_width&&*state->available_width==bounds.width)return;
                if(state->revision==std::numeric_limits<std::uint64_t>::max()){state->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"TabBar revision exhausted"};return;}
                ++state->revision;
                state->available_width=bounds.width;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();
            });!r)return std::unexpected(r.error());
            return NativeTabBar(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"TabBar creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TabBarConfiguration> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());
        if(auto collected=collect(state_);!collected)return std::unexpected(collected.error());return state_->model.configuration();
    }
    [[nodiscard]] ii::core::Result<void> set_options(TabBarConfiguration value)const{return apply(state_,value);}
    [[nodiscard]] ii::core::Result<void> status()const{
        if(auto valid=check(state_);!valid)return valid;if(state_->failure)return std::unexpected(*state_->failure);return {};
    }
    [[nodiscard]] ii::core::Result<void> register_tab(TabGroupPort port)const{
        if(auto valid=check(state_);!valid)return valid;
        if(!port.present||!port.activated||!port.changed||!port.availability||!port.minimum_width||!port.snapshot||!port.focus||!port.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete TabBar child port");
        bool action_attached=false,observer_attached=false;
        try{
            if(auto collected=collect(state_);!collected)return collected;
            if(state_->entries.size()>=4096||state_->next_id==std::numeric_limits<std::uint64_t>::max()||state_->revision==std::numeric_limits<std::uint64_t>::max())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar child or revision limit exceeded");
            auto snapshot=port.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            auto parent=state_->root.snapshot();if(!parent)return std::unexpected(parent.error());
            if(snapshot->parent_id!=parent->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"TabBar currently accepts direct Tab children only");
            if(std::ranges::any_of(state_->entries,[&](const auto& entry){return entry.key==snapshot->id;}))return ii::core::fail(ii::core::ErrorCode::already_exists,"Tab is already registered");
            const auto id=++state_->next_id;std::weak_ptr<State> weak=state_;
            state_->entries.reserve(state_->entries.size()+1);State::Entry entry{id,snapshot->id,port,{}};
            if(auto attached=port.activated([weak,id]{auto state=weak.lock();if(!state)return false;auto result=activate(state,id);
                    if(result){state->failure.reset();return true;}state->failure=result.error();return false;});!attached)return attached;
            action_attached=true;
            if(auto attached=port.changed([weak]{if(auto state=weak.lock()){
                    if(state->revision==std::numeric_limits<std::uint64_t>::max()){state->failure=ii::core::Error{ii::core::ErrorCode::out_of_range,"TabBar revision exhausted"};return;}
                    ++state->revision;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();}});!attached){(void)port.activated({});return attached;}
            observer_attached=true;
            ++state_->revision;
            state_->entries.push_back(std::move(entry));
            if(auto result=sync(state_);!result){std::erase_if(state_->entries,[&](const auto& entry){return entry.id==id;});(void)port.activated({});(void)port.changed({});(void)sync(state_);return result;}
            return {};
        }catch(...){auto cause=std::current_exception();try{if(observer_attached)(void)port.changed({});if(action_attached)(void)port.activated({});}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"TabBar registration failed",cause);}
    }
    // Initial construction may register zero or more children before selection
    // is validated. Later registrations and retirements use their stable IDs.
    [[nodiscard]] ii::core::Result<void> finalize()const{
        if(auto valid=check(state_);!valid)return valid;
        if(auto collected=collect(state_);!collected)return collected;
        auto config=state_->model.configuration();
        if(config.current_index>=0){
            const auto index=std::size_t(config.current_index);
            if(index>=state_->entries.size()){
                if(!state_->entries.empty())return ii::core::fail(ii::core::ErrorCode::out_of_range,"TabBar selection is outside its children");
                config.current_index=-1;
            }
            else{auto candidate=state_->entries[index].port.availability();if(!candidate)return std::unexpected(candidate.error());
                if(!candidate->enabled||!candidate->visible){auto first=state_->model.focus_target(std::nullopt,TabFocusMove::first);config.current_index=first?int(*first):-1;}}
        }
        const bool previous=state_->finalized;state_->finalized=true;auto result=apply(state_,config);if(!result)state_->finalized=previous;return result;
    }
    [[nodiscard]] TabRegistrationPort weak_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](TabGroupPort child)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"TabBar has retired");
            return NativeTabBar(std::move(state)).register_tab(std::move(child));};
    }
};
} // namespace lvui::native
