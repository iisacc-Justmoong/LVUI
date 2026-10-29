#pragma once

#include "ListPanel.h"
#include "NativeListFooter.h"
#include "NativeListToolbar.h"
#include "NativeListView.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

[[nodiscard]] inline StyleColor list_panel_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct ListPanelInput final {
    ListPanelConfiguration appearance;
    ListViewInput list;
    ListToolbarInput toolbar;
    ListFooterInput footer;
    friend bool operator==(const ListPanelInput&,const ListPanelInput&)=default;
};
struct NativeListPanelCallbacks final {
    std::function<void(const ListViewEvent&)> item_activated;
    std::function<void(std::size_t,std::string)> toolbar_clicked;
    std::function<void(std::size_t,const ListFooterSlot&)> footer_clicked;
};

class NativeListPanel final {
    struct Effective final {ListViewInput list;ListToolbarInput toolbar;ListFooterInput footer;PanelRegions geometry;};
    struct State final {
        ListPanelInput authored;Effective effective;
        ii::gui::Widget root;
        std::optional<NativeListView> list;
        std::optional<NativeListToolbar> toolbar;
        std::optional<NativeListFooter> footer;
        std::shared_ptr<ListController> owned_controller;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        bool live=false,applying=false,pending=false;
    };
    explicit NativeListPanel(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::Rect rect(PanelRegionRect value)noexcept{return {float(value.x),float(value.y),float(value.width),float(value.height)};}
    [[nodiscard]] static ii::gui::Color color(StyleColor value)noexcept{
        const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(linear(value.red)),float(linear(value.green)),float(linear(value.blue)),float(value.alpha)};
    }
    [[nodiscard]] static ii::core::Result<Effective> effective(const ListPanelInput& input,ii::gui::Size extent){
        auto geometry=list_panel_geometry(input.appearance,extent.width,extent.height);
        if(!geometry||!input.list.controller)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list panel configuration");
        Effective result{input.list,input.toolbar,input.footer,*geometry};const auto width=std::max(1.,extent.width);
        result.list.width=width;result.list.height=std::max(1.,geometry->list.height);
        result.list.enabled=result.list.enabled&&input.appearance.enabled;result.list.interactive=result.list.interactive&&input.appearance.interactive;
        result.toolbar.configuration.width=width;result.toolbar.configuration.height=std::max(1.,geometry->toolbar.height);
        result.toolbar.configuration.enabled=result.toolbar.configuration.enabled&&input.appearance.enabled;
        result.toolbar.configuration.interactive=result.toolbar.configuration.interactive&&input.appearance.interactive;
        result.footer.configuration.width=width;result.footer.configuration.height=std::max(1.,geometry->footer.height);
        result.footer.configuration.enabled=result.footer.configuration.enabled&&input.appearance.enabled;
        result.footer.configuration.interactive=result.footer.configuration.interactive&&input.appearance.interactive;
        if(!ListToolbar::validate(result.toolbar.configuration)||!ListFooter::validate(result.footer.configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list panel child appearance");
        return result;
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state,const PanelRegions& geometry){
        const auto set=[](ii::gui::Widget widget,PanelRegionRect frame,bool visible)->ii::core::Result<void>{
            if(auto r=widget.set_visible(visible);!r)return r;return widget.set_frame(rect(frame));};
        if(auto r=set(state->toolbar->root(),geometry.toolbar,geometry.toolbar_visible);!r)return r;
        if(auto r=set(state->list->root(),geometry.list,geometry.list_visible);!r)return r;
        return set(state->footer->root(),geometry.footer,geometry.footer_visible);
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,const ListPanelInput& input,ii::gui::Size extent){
        auto next=effective(input,extent);if(!next)return std::unexpected(next.error());const auto previous=state->effective;
        const auto restore=[&]{(void)state->toolbar->set_options(previous.toolbar);(void)state->list->set_options(previous.list);
            (void)state->footer->set_options(previous.footer);(void)frames(state,previous.geometry);};
        if(next->toolbar!=previous.toolbar)if(auto r=state->toolbar->set_options(next->toolbar);!r)return r;
        if(next->list!=previous.list)if(auto r=state->list->set_options(next->list);!r){restore();return r;}
        if(next->footer!=previous.footer)if(auto r=state->footer->set_options(next->footer);!r){restore();return r;}
        if(next->geometry!=previous.geometry)if(auto r=frames(state,next->geometry);!r){restore();return r;}
        state->effective=std::move(*next);return {};
    }
    [[nodiscard]] static ii::core::Result<void> settle(const std::shared_ptr<State>& state){
        for(unsigned pass=0;pass<8;++pass){state->pending=false;auto result=apply(state,state->authored,state->extent);
            if(!result)return result;if(!state->pending)return {};}
        return ii::core::fail(ii::core::ErrorCode::busy,"Flat list panel layout did not settle");
    }
    [[nodiscard]] static ii::core::Result<void> ready(const std::shared_ptr<State>& state,bool mutation=false){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list panel requires its owner thread");
        if(mutation&&state->applying)return ii::core::fail(ii::core::ErrorCode::busy,"Flat list panel is updating");
        if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeListPanel> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ListPanelInput input={},NativeListPanelCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();
            if(!input.list.controller){auto memory=ListMemory::create();if(!memory)return ii::core::fail(ii::core::ErrorCode::invalid_argument,memory.error().message);
                auto controller=ListController::create(*memory);if(!controller)return ii::core::fail(ii::core::ErrorCode::invalid_argument,controller.error().message);
                state->owned_controller=std::make_shared<ListController>(std::move(*controller));input.list.controller=state->owned_controller;}
            state->authored=std::move(input);state->extent={state->authored.appearance.width,state->authored.appearance.height};
            auto prepared=effective(state->authored,state->extent);if(!prepared)return std::unexpected(prepared.error());state->effective=*prepared;
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto toolbar=NativeListToolbar::create(builder,*root,std::string(id)+".toolbar",prepared->toolbar,{.clicked=std::move(callbacks.toolbar_clicked)});
            if(!toolbar)return std::unexpected(toolbar.error());state->toolbar=*toolbar;
            auto list=NativeListView::create(builder,*root,std::string(id)+".list",prepared->list,{.activated=std::move(callbacks.item_activated)});
            if(!list)return std::unexpected(list.error());state->list=*list;
            auto footer=NativeListFooter::create(builder,*root,std::string(id)+".footer",prepared->footer,{.clicked=std::move(callbacks.footer_clicked)});
            if(!footer)return std::unexpected(footer.error());state->footer=*footer;
            if(auto r=frames(state,prepared->geometry);!r)return std::unexpected(r.error());std::weak_ptr<State> weak=state;
            if(auto r=root->set_paint_handler([weak](const ii::gui::WidgetPaintState& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list panel retired");
                auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto fill=color(state->authored.appearance.background);
                if(fill.alpha>0&&view.size.width>0&&view.size.height>0)drawing->panels.push_back({{0,0,float(view.size.width),float(view.size.height)},fill,fill,0});
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));});!r)return std::unexpected(r.error());
            if(auto r=root->set_enabled(state->authored.appearance.enabled);!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};if(!state->live)return;
                if(state->applying){state->pending=true;return;}state->applying=true;auto result=settle(state);state->applying=false;
                if(!result)state->failure=result.error();else state->failure.reset();}});!r)return std::unexpected(r.error());
            if(auto r=builder.retain(state);!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto state=weak.lock()){state->live=true;state->applying=true;
                auto result=settle(state);state->applying=false;if(!result)state->failure=result.error();}});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!r)return std::unexpected(r.error());
            return NativeListPanel(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list panel construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] NativeListView list()const{return *state_->list;}
    [[nodiscard]] NativeListToolbar toolbar()const{return *state_->toolbar;}
    [[nodiscard]] NativeListFooter footer()const{return *state_->footer;}
    [[nodiscard]] ii::core::Result<ListPanelInput> options()const{if(auto r=ready(state_);!r)return std::unexpected(r.error());return state_->authored;}
    [[nodiscard]] ii::core::Result<PanelRegions> geometry()const{if(auto r=ready(state_);!r)return std::unexpected(r.error());return state_->effective.geometry;}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{if(auto r=ready(state_);!r)return std::unexpected(r.error());return state_->failure;}
    [[nodiscard]] ii::core::Result<void> set_options(ListPanelInput input)const{
        if(auto r=ready(state_,true);!r)return r;if(!input.list.controller)input.list.controller=state_->owned_controller;if(input==state_->authored)return {};
        if(!effective(input,{input.appearance.width,input.appearance.height}))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list panel configuration");
        try{state_->applying=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{state_->applying};
            const auto previous=state_->authored;const auto old_extent=state_->extent;state_->authored=std::move(input);
            state_->extent={state_->authored.appearance.width,state_->authored.appearance.height};
            const auto rollback=[&]{state_->authored=previous;state_->extent=old_extent;(void)state_->root.set_extent({previous.appearance.width,previous.appearance.height});
                (void)state_->root.set_enabled(previous.appearance.enabled);(void)settle(state_);};
            if(auto r=state_->root.set_extent(state_->extent);!r){rollback();return r;}
            if(auto r=state_->root.set_enabled(state_->authored.appearance.enabled);!r){rollback();return r;}
            if(auto r=settle(state_);!r){rollback();return r;}if(auto r=state_->root.request_repaint();!r){rollback();return r;}
            state_->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list panel update failed",std::current_exception());}
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
