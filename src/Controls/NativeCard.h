#pragma once

#include <LVUI/Card.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeVisual.h>

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
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

[[nodiscard]] inline StyleColor card_color(ii::gui::WidgetColor value) noexcept {return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline CardType card_type(CardType value) noexcept {return value;}
[[nodiscard]] inline CardSize card_size(CardSize value) noexcept {return value;}
[[nodiscard]] inline CardDetail card_detail(CardDetail value) noexcept {return value;}
[[nodiscard]] inline CardDisplayState card_display_state(CardDisplayState value) noexcept {return value;}
struct CardInput final {
    CardConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon, preview;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const CardInput&,const CardInput&) = default;
};
struct NativeCardCallbacks final {
    std::function<void()> clicked;
    std::function<void(bool)> selected_changed;
    std::function<void()> menu_requested, action_triggered;
};

class NativeCard final {
    struct Prepared final { CardInput authored; Card model; std::shared_ptr<const ii::gui::RasterImage> icon, preview; };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        NativeIconCache icons;
        ii::gui::Widget root,surface,menu,action;
        NativeCardCallbacks callbacks;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit NativeCard(std::shared_ptr<State> state):state_(std::move(state)) {}
    [[nodiscard]] static ii::gui::Rect rect(CardRect value) noexcept { return {float(value.x),float(value.y),float(value.width),float(value.height)}; }
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const CardInput& input,State& state) {
        if (!Card::validate(input.configuration) || input.icon_revision < 0 || (input.icon && !input.configuration.icon_name.empty()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card configuration");
        try {
            auto next=std::make_shared<Prepared>(); next->authored=input;auto c=input.configuration;
            const auto bound=[](std::string& value){value=native_display_text(value);};
            bound(c.title);bound(c.description);bound(c.details);bound(c.metadata);bound(c.status_text);bound(c.summary);
            bound(c.footnote);bound(c.domain);bound(c.preview_title);bound(c.progress_text);bound(c.menu_accessibility_name);
            if(c.action_text)bound(*c.action_text);if(c.progress_label)bound(*c.progress_label);
            for(auto& row:c.rows){bound(row.label);bound(row.value);}
            if(!next->model.set_configuration(std::move(c)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card display state");
            next->icon=input.icon;next->preview=input.preview;
            if(!input.configuration.icon_name.empty()){
                auto image=state.icons.resolve(input.configuration.icon_name,input.icon_source,40);
                if(!image)return std::unexpected(image.error());next->icon=std::move(*image);
            }
            return Snapshot(std::move(next));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Card preparation failed",std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::ActionOptions surface_options(const Snapshot& value) {
        const auto& c=value->model.configuration();const auto size=value->model.intrinsic_size();
        const auto name=!c.title.empty()?c.title:!c.preview_title.empty()?c.preview_title:c.domain;
        return {.accessibility_name=name,.enabled=c.enabled,.focus_ring=false,.width=size.width,.height=size.height,
                .checkable=value->model.can_select(),.checked=value->model.can_select()&&c.selected,.accessibility_description=c.description};
    }
    [[nodiscard]] static ii::gui::ActionOptions menu_options(const Snapshot& value) {
        const auto& c=value->model.configuration();return {.accessibility_name=c.menu_accessibility_name,.enabled=c.enabled,.focus_ring=false};
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const Snapshot& value) {
        const auto name=value->model.action_text();
        return {.accessibility_name=name.empty()?"Action":std::string(name),.enabled=value->model.configuration().enabled,.focus_ring=false};
    }
    [[nodiscard]] static ii::gui::Rect crop(const ii::gui::RasterImage& image,CardRect target) noexcept {
        const auto source_ratio=double(image.width())/double(image.height());const auto target_ratio=target.width/target.height;
        if(source_ratio>target_ratio){const auto width=target_ratio/source_ratio;return {float((1-width)/2),0,float(width),1};}
        const auto height=source_ratio/target_ratio;return {0,float((1-height)/2),1,float(height)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_surface(const Snapshot& value,
            const ii::gui::WidgetPaintState& state) {
        try {
            const auto geometry=value->model.geometry(state.size.width,state.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card paint extent");
            const auto& g=*geometry;const auto& c=value->model.configuration();auto visual=value->model.visual({state.hovered,state.pressed,state.focused});
            if(!state.enabled){visual.text=c.palette.disabled_text;visual.secondary=c.palette.disabled_text;visual.focused=false;}
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto radius=float(std::min({c.corner_radius,state.size.width/2,state.size.height/2}));
            const auto surface=native_encoded_color(visual.surface),border=native_encoded_color(visual.border);
            if(g.surface.width>0&&g.surface.height>0){
                drawing->panels.push_back({rect(g.surface),surface,surface,radius});
                if(c.type!=CardType::file_preview)drawing->panels.push_back({rect(g.surface),border,border,radius,visual.selected?2.f:1.f});
                if(visual.focused){const auto focus=native_encoded_color(visual.focus);drawing->panels.push_back({rect(g.surface),focus,focus,radius,1.5f});}
            }
            if(value->preview&&g.preview.width>0&&g.preview.height>0)
                drawing->images.push_back({.bounds=rect(g.preview),.image=value->preview,.opacity=state.enabled?1.f:.35f,
                                           .source=crop(*value->preview,g.preview),.radius=radius});
            if(value->icon&&(!value->preview||c.type!=CardType::file)&&g.icon.width>0&&g.icon.height>0)
                drawing->images.push_back({.bounds=rect(g.icon),.image=value->icon,.opacity=state.enabled?1.f:.35f});
            const auto clip=rect(g.surface);
            const auto text=[&](CardRect area,std::string_view label,StyleColor color,float size,unsigned weight,ii::gui::TextAlignment align=ii::gui::TextAlignment::left){
                if(area.width<=0||area.height<=0||label.empty())return;
                drawing->texts.push_back({.bounds=rect(area),.text=std::string(label),.color=native_encoded_color(color),.font_size=size,.font_weight=weight,
                    .alignment=align,.font_family=state.font_family,.line_height=float(area.height),.elide_right=true,.clip=clip});
            };
            text(g.title,c.type==CardType::file_preview?c.preview_title:c.title,visual.text,14,600);text(g.description,c.description,visual.secondary,12,400);
            text(g.details,c.details,visual.secondary,11,400);text(g.metadata,c.metadata,visual.secondary,11,400);
            text(g.status,c.status_text,state.enabled?c.palette.status:c.palette.disabled_text,11,500);
            text(g.summary,c.summary,visual.secondary,11,400);text(g.footnote,c.footnote,visual.secondary,11,400);text(g.domain,c.domain,visual.secondary,11,400);
            for(const auto& row:g.rows){text(row.label,c.rows[row.index].label,visual.secondary,11,400);text(row.value,c.rows[row.index].value,visual.text,11,500,ii::gui::TextAlignment::right);}
            text(g.progress_label,value->model.progress_label(),visual.secondary,11,400);
            text(g.progress_text,c.progress_text,visual.text,11,500,ii::gui::TextAlignment::right);
            if(g.progress_track.width>0&&g.progress_track.height>0){const auto track=native_encoded_color(c.palette.progress_track),fill=native_encoded_color(c.palette.progress);
                drawing->panels.push_back({rect(g.progress_track),track,track,float(g.progress_track.height/2)});
                if(g.progress_fill.width>0)drawing->panels.push_back({rect(g.progress_fill),fill,fill,float(g.progress_fill.height/2)});
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_button(
            const Snapshot& value,const ii::gui::WidgetPaintState& state,bool menu) {
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& c=value->model.configuration();
            const auto background=state.pressed?c.palette.selected:state.hovered?c.palette.hover:c.palette.surface;
            const auto fill=native_encoded_color(background),focus=native_encoded_color(c.palette.focus);
            const auto radius=float(std::min({6.,state.size.width/2,state.size.height/2}));
            const ii::gui::Rect bounds{0,0,float(state.size.width),float(state.size.height)};
            if(bounds.width>0&&bounds.height>0){drawing->panels.push_back({bounds,fill,fill,radius});
                if(state.enabled&&state.focused)drawing->panels.push_back({bounds,focus,focus,radius,1.5f});}
            const auto label=menu?std::string("⋯"):std::string(value->model.action_text());
            if(!label.empty()&&bounds.width>0&&bounds.height>0)drawing->texts.push_back({.bounds=bounds,.text=label,
                .color=native_encoded_color(state.enabled?c.palette.text:c.palette.disabled_text),.font_size=menu?16.f:12.f,.font_weight=500,
                .alignment=ii::gui::TextAlignment::center,.font_family=state.font_family,.line_height=float(state.size.height),.elide_right=true,.clip=bounds});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card action paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state) {
        const auto g=state->prepared->model.geometry(state->extent.width,state->extent.height);
        if(!g)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card layout extent");
        if(auto r=state->surface.set_frame(rect(g->surface));!r)return r;
        const bool menu=state->prepared->model.shows_menu()&&g->menu.width>0&&g->menu.height>0;
        const bool action=state->prepared->model.shows_action()&&g->action.width>0&&g->action.height>0;
        if(auto r=state->menu.set_visible(menu);!r)return r;if(menu)if(auto r=state->menu.set_frame(rect(g->menu));!r)return r;
        if(auto r=state->action.set_visible(action);!r)return r;if(action)if(auto r=state->action.set_frame(rect(g->action));!r)return r;
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state,std::uint64_t revision) {
        const auto step=[&](auto&& action)->ii::core::Result<bool>{if(state->revision!=revision)return false;auto result=action();
            if(!result)return std::unexpected(result.error());return state->revision==revision;};
        const auto size=state->prepared->model.intrinsic_size();
        auto result=step([&]{return state->root.set_extent({size.width,size.height});});if(!result||!*result)return result;
        result=step([&]{return state->root.set_visible(state->prepared->authored.configuration.visible);});if(!result||!*result)return result;
        result=step([&]{return state->surface.set_action_options(surface_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return state->menu.set_action_options(menu_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return state->action.set_action_options(action_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return frames(state);});if(!result||!*result)return result;
        result=step([&]{return state->surface.request_repaint();});if(!result||!*result)return result;
        result=step([&]{return state->menu.request_repaint();});if(!result||!*result)return result;
        return step([&]{return state->action.request_repaint();});
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,CardInput input) {
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        try {
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(state->prepared->authored==input)return {};
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Card revision exhausted");
            const auto observed=state->revision;auto next=prepare(input,*state);if(!next)return std::unexpected(next.error());
            if(!state->live||state->revision!=observed)return {};
            auto previous=state->prepared;const auto old_extent=state->extent;const auto revision=++state->revision;state->prepared=std::move(*next);
            const auto size=state->prepared->model.intrinsic_size();state->extent={size.width,size.height};
            auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->prepared=std::move(previous);state->extent=old_extent;(void)apply(state,revision);}return std::unexpected(result.error());}
            state->failure.reset();return {};
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card update failed",std::current_exception());}
    }
    static void activate(const std::shared_ptr<State>& state) noexcept {
        try {
            if(!state->live || !state->prepared->authored.configuration.enabled || !state->prepared->authored.configuration.visible)return;
            auto model=state->prepared->model;const auto selected=model.toggle_selection();auto revision=state->revision;
            if(selected){auto input=state->prepared->authored;input.configuration.selected=*selected;auto changed=update(state,std::move(input));
                if(!changed){state->failure=changed.error();return;}if(!state->live||state->revision!=revision+1)return;++revision;
                if(state->callbacks.selected_changed){auto callback=state->callbacks.selected_changed;callback(*selected);}}
            if(state->live&&state->revision==revision&&state->callbacks.clicked){auto callback=state->callbacks.clicked;callback();}
        } catch (...) {try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Card activation failed",std::current_exception()};}catch(...) {}}
    }
    static void trigger(const std::shared_ptr<State>& state,bool menu) noexcept {
        try {if(!state->live||!state->prepared->authored.configuration.enabled||!state->prepared->authored.configuration.visible)return;
            auto callback=menu?state->callbacks.menu_requested:state->callbacks.action_triggered;if(callback)callback();
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Card action failed",std::current_exception()};}catch(...) {}}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeCard> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            CardInput input={},NativeCardCallbacks callbacks={}) {
        try {
            auto state=std::make_shared<State>();auto prepared=prepare(input,*state);if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);const auto size=state->prepared->model.intrinsic_size();
            state->extent={size.width,size.height};std::weak_ptr<State> weak=state;const auto name=[&](std::string_view suffix){return std::string(id)+"."+std::string(suffix);};
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto surface=builder.action_surface(*root,name("surface"),surface_options(state->prepared),[weak]{if(auto state=weak.lock())activate(state);});
            if(!surface)return std::unexpected(surface.error());state->surface=*surface;
            if(auto paint=surface->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_surface(state->prepared,view);
            });!paint)return std::unexpected(paint.error());
            auto menu=builder.action_surface(*root,name("menu"),menu_options(state->prepared),[weak]{if(auto state=weak.lock())trigger(state,true);});
            if(!menu)return std::unexpected(menu.error());state->menu=*menu;
            if(auto paint=menu->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_button(state->prepared,view,true);
            });!paint)return std::unexpected(paint.error());
            auto action=builder.action_surface(*root,name("action"),action_options(state->prepared),[weak]{if(auto state=weak.lock())trigger(state,false);});
            if(!action)return std::unexpected(action.error());state->action=*action;
            if(auto paint=action->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_button(state->prepared,view,false);
            });!paint)return std::unexpected(paint.error());
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            if(auto layout=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};
                auto result=frames(state);if(!result)state->failure=result.error();
            }});!layout)return std::unexpected(layout.error());
            auto applied=apply(state,state->revision);if(!applied)return std::unexpected(applied.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!retired)return std::unexpected(retired.error());
            return NativeCard(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Card construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget surface()const{return state_->surface;}
    [[nodiscard]] ii::gui::Widget menu()const{return state_->menu;}
    [[nodiscard]] ii::gui::Widget action()const{return state_->action;}
    [[nodiscard]] ii::core::Result<CardInput> options()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->authored;
    }
    [[nodiscard]] ii::core::Result<void> set_options(CardInput input)const{return update(state_,std::move(input));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
