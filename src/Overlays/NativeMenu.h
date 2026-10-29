#pragma once
#include <LVUI/Menu.h>
#include <LVUI/NativeDivider.h>
#include <LVUI/NativeMenuItem.h>
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
#include <vector>
namespace lvui::native {
[[nodiscard]] inline StyleColor menu_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct MenuInput final {
    MenuConfiguration configuration;
    int selected_index=-1;
    friend bool operator==(const MenuInput&,const MenuInput&)=default;
};
struct NativeMenuCallbacks final {std::function<void(int)> triggered;std::function<void()> opened,closed,canceled;};
using MenuRegistrationPort=std::function<ii::core::Result<void>(MenuItemPort)>;
class NativeMenu final {
    struct State final {
        struct Entry final {std::uint64_t id;std::string widget_id,key;MenuItemPort port;MenuItemPresentation applied;};
        Menu model;
        MenuInput authored;
        ii::gui::Widget root,content;
        std::vector<Entry> entries;
        NativeMenuCallbacks callbacks;
        MenuCloseAction parent_close;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_id=0;
        bool live=false,finalized=false,subordinate=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeMenu(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Error convert(const MenuError& error){
        const auto code=error.code==MenuErrorCode::closed?ii::core::ErrorCode::closed:
            error.code==MenuErrorCode::duplicate?ii::core::ErrorCode::already_exists:
            error.code==MenuErrorCode::limit?ii::core::ErrorCode::out_of_range:
            error.code==MenuErrorCode::stale?ii::core::ErrorCode::cancelled:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Menu requires its owner thread");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static ii::gui::StackOptions layout(const MenuConfiguration& c){
        ii::gui::StackOptions result;result.spacing=c.item_spacing;result.alignment=ii::gui::StackAlignment::stretch;
        result.insets=ii::gui::Insets{c.left_padding,c.top_padding,c.right_padding,c.bottom_padding};return result;
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const MenuConfiguration& c){
        return {.role=ii::gui::AccessibilityRole::menu,.name=c.accessibility_name};
    }
    [[nodiscard]] static ii::gui::PopoverOptions popup_options(const MenuConfiguration& c,const MenuLayout& extent){
        ii::gui::PopoverOptions value;value.width=extent.width;value.height=std::max(1.,extent.height);value.padding=0;value.margins=c.edge_margin;
        value.corner_radius=c.corner_radius;value.surface_opacity=0;value.opaque_base=false;value.fit_to_viewport=true;value.clip_content=true;
        value.dismiss_on_outside_press=c.dismiss_outside;value.dismiss_on_context_request=c.dismiss_context;value.accessibility_name=c.accessibility_name;return value;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        try{
            if(!std::isfinite(input.size.width)||!std::isfinite(input.size.height)||input.size.width<0||input.size.height<0||
               input.size.width>8192||input.size.height>8192)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu paint extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();const auto& c=state->model.configuration();
            const ii::gui::Rect bounds{0,0,float(input.size.width),float(input.size.height)};
            if(bounds.width>0&&bounds.height>0){const auto radius=float(std::min({c.corner_radius,input.size.width/2,input.size.height/2}));
                auto ink=linear(c.base);if(ink.alpha>0)result->panels.push_back({bounds,ink,ink,radius});
                auto background=c.background;background.alpha*=c.opacity;ink=linear(background);
                if(ink.alpha>0)result->panels.push_back({bounds,ink,ink,float(std::min({c.corner_radius,input.size.width/2,input.size.height/2}))});
                ink=linear(c.border);if(ink.alpha>0)
                    result->panels.push_back({bounds,ink,ink,float(std::min({c.corner_radius,input.size.width/2,input.size.height/2})),1});}
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> present(const MenuItemPort& port,MenuItemPresentation value){
        try{return port.present(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu item presentation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<MenuCloseAction> close_plan(const std::shared_ptr<State>& state){
        if(auto valid=check(state);!valid)return std::unexpected(valid.error());
        auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        if(!state->live||!snapshot->opened||!state->model.is_open())return ii::core::fail(ii::core::ErrorCode::closed,"Parent menu is closed");
        const auto epoch=state->model.epoch();const auto ancestor=state->parent_close;std::weak_ptr<State> weak=state;
        return MenuCloseAction{[weak,epoch,ancestor]()->ii::core::Result<void>{
            auto value=weak.lock();if(!value)return {};
            if(auto valid=check(value);!valid)return valid;
            auto current=value->root.snapshot();if(!current)return std::unexpected(current.error());
            if(!value->live||!current->opened||value->model.epoch()!=epoch)return {};
            if(auto closed=value->root.close_popup();!closed)return closed;
            current=value->root.snapshot();if(!current)return std::unexpected(current.error());
            if(!current->opened&&value->model.epoch()==epoch&&ancestor)return ancestor();return {};
        }};
    }
    [[nodiscard]] static ii::core::Result<void> sync(const std::shared_ptr<State>& state){
        try{
            if(auto valid=check(state);!valid)return valid;
            std::erase_if(state->entries,[](const auto& entry){return entry.port.retired&&entry.port.retired();});
            std::vector<MenuEntry> items;items.reserve(state->entries.size());
            for(const auto& item:state->entries){auto value=item.port.entry();if(!value)return std::unexpected(value.error());
                value->key=item.key;value->compact=state->authored.configuration.compact_items;items.push_back(std::move(*value));}
            if(auto updated=state->model.set_entries(std::move(items));!updated)return std::unexpected(convert(updated.error()));
            const auto extent=state->model.layout();if(!extent||extent->height>8192)
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu extent exceeds the host limit");
            const auto revision=state->revision;const auto c=state->authored.configuration;
            if(auto result=state->content.set_stack_options(layout(c));!result)return result;
            if(state->revision!=revision)return {};
            if(auto result=state->content.set_accessibility_semantics(semantics(c));!result)return result;
            if(state->revision!=revision)return {};
            if(auto result=state->root.set_popover_options(popup_options(c,*extent));!result)return result;
            if(state->revision!=revision)return {};
            const auto entries=state->entries;std::vector<std::size_t> changed;changed.reserve(entries.size());
            const auto mark=[&](std::uint64_t id,MenuItemPresentation value){auto item=std::ranges::find_if(state->entries,[&](const auto& entry){return entry.id==id;});
                if(item!=state->entries.end())item->applied=value;};
            for(std::size_t index=0;index<entries.size();++index){const auto& item=entries[index];
                MenuItemPresentation value{.selected=state->model.selected_index()<0?std::nullopt:std::optional<bool>{state->model.selected_index()==int(index)},
                    .compact=c.compact_items,.show_icon_slot=c.show_icon_slot,.width=c.item_width};
                if(item.applied==value)continue;auto result=present(item.port,value);if(state->revision!=revision)return {};
                if(!result){for(auto prior=changed.rbegin();prior!=changed.rend()&&state->revision==revision;++prior)
                        if(auto restored=present(entries[*prior].port,entries[*prior].applied);restored&&state->revision==revision)mark(entries[*prior].id,entries[*prior].applied);
                    return result;}
                changed.push_back(index);mark(item.id,value);
            }
            if(state->revision!=revision)return {};return state->content.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu synchronization failed",std::current_exception());}
    }
    static void activated(const std::weak_ptr<State>& weak,std::uint64_t id,std::function<void()> child){
        auto state=weak.lock();if(!state||!state->live)return;
        try{
            auto popup=state->root.snapshot();if(!popup||!popup->opened){state->failure=ii::core::Error{ii::core::ErrorCode::closed,"Menu is closed"};return;}
            if(!state->model.is_open()){auto opened=state->model.open();if(!opened){state->failure=convert(opened.error());return;}}
            const auto entry=std::ranges::find_if(state->entries,[&](const auto& value){return value.id==id;});
            if(entry==state->entries.end()){state->failure=ii::core::Error{ii::core::ErrorCode::closed,"Menu item retired"};return;}
            const auto index=std::size_t(entry-state->entries.begin());auto plan=state->model.prepare_activation(index);
            if(!plan){state->failure=convert(plan.error());return;}
            if(child)child();if(!state->live)return;
            auto still=state->root.snapshot();if(!still||!still->opened)return;
            auto triggered=state->callbacks.triggered;if(triggered)triggered(int(index));if(!state->live)return;
            still=state->root.snapshot();if(!still||!still->opened)return;
            auto completed=state->model.complete_activation(*plan);
            if(!completed){if(completed.error().code!=MenuErrorCode::stale)state->failure=convert(completed.error());return;}
            if(*completed){const auto epoch=state->model.epoch();auto parent=state->parent_close;
                auto closed=state->root.close_popup();if(!closed){state->failure=closed.error();return;}
                auto current=state->root.snapshot();if(!current){state->failure=current.error();return;}
                if(!current->opened&&state->model.epoch()==epoch&&parent)if(auto result=parent();!result)state->failure=result.error();}
        }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Menu activation failed",std::current_exception()};}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(event.kind!=WidgetInputKind::key_down)return false;
        if(event.key==WidgetKey::left&&state->subordinate){auto result=state->root.close_popup();if(!result)return std::unexpected(result.error());return true;}
        if(event.key==WidgetKey::right){
            for(const auto& entry:state->entries){auto focused=entry.port.snapshot();if(!focused)return std::unexpected(focused.error());
                if(focused->focused){auto opened=entry.port.open_submenu();if(!opened)return std::unexpected(opened.error());return *opened;}}
            return false;
        }
        MenuNavigation direction;
        switch(event.key){case WidgetKey::up:direction=MenuNavigation::previous;break;case WidgetKey::down:direction=MenuNavigation::next;break;
            case WidgetKey::home:direction=MenuNavigation::first;break;case WidgetKey::end:direction=MenuNavigation::last;break;default:return false;}
        try{
            if(auto valid=check(state);!valid)return std::unexpected(valid.error());
            auto popup=state->root.snapshot();if(!popup)return std::unexpected(popup.error());if(!popup->opened)return false;
            if(!state->model.is_open()){auto opened=state->model.open();if(!opened)return std::unexpected(convert(opened.error()));}
            if(direction==MenuNavigation::previous||direction==MenuNavigation::next){
                for(std::size_t i=0;i<state->entries.size();++i){auto focused=state->entries[i].port.snapshot();
                    if(!focused)return std::unexpected(focused.error());
                    if(focused->focused&&state->model.entries()[i].kind!=MenuEntryKind::divider){auto selected=state->model.select(int(i));if(!selected)return std::unexpected(convert(selected.error()));break;}}
            }
            auto index=state->model.navigate(direction);if(!index)return std::unexpected(convert(index.error()));
            if(auto refreshed=sync(state);!refreshed)return std::unexpected(refreshed.error());
            if(*index>=0&&std::size_t(*index)<state->entries.size()){auto result=state->entries[std::size_t(*index)].port.focus();if(!result)return std::unexpected(result.error());}
            return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu keyboard handling failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeMenu> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,MenuInput input={},NativeMenuCallbacks callbacks={}){
        if(!Menu::validate(input.configuration)||input.selected_index < -1||input.selected_index>=4096)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Menu configuration");
        try{
            auto state=std::make_shared<State>();state->authored=input;state->callbacks=std::move(callbacks);
            if(auto configured=state->model.set_configuration(input.configuration);!configured)return std::unexpected(convert(configured.error()));
            auto initial=state->model.layout();if(!initial)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Invalid menu extent");
            std::weak_ptr<State> weak=state;
            auto opened=[weak]{if(auto value=weak.lock();value&&value->live){auto current=value->root.snapshot();if(!current||!current->opened)return;
                auto result=value->model.open();if(!result){value->failure=convert(result.error());return;}
                auto selected=value->model.selected_index();
                if(selected<0)for(std::size_t i=0;i<value->model.entries().size();++i){const auto& entry=value->model.entries()[i];
                    if(entry.visible&&entry.enabled&&entry.kind!=MenuEntryKind::divider&&entry.state!=MenuEntryState::inactive){selected=int(i);break;}}
                if(selected>=0&&std::size_t(selected)<value->entries.size())if(auto focused=value->entries[std::size_t(selected)].port.focus();!focused){value->failure=focused.error();return;}
                if(!value->live)return;current=value->root.snapshot();if(!current||!current->opened)return;auto callback=value->callbacks.opened;if(callback)callback();}};
            auto closed=[weak]{if(auto value=weak.lock();value&&value->live){auto current=value->root.snapshot();if(!current||current->opened)return;
                auto result=value->model.close();if(!result){value->failure=convert(result.error());return;}value->parent_close={};value->subordinate=false;auto callback=value->callbacks.closed;if(callback)callback();}};
            auto canceled=[weak]{if(auto value=weak.lock();value&&value->live){auto callback=value->callbacks.canceled;if(callback)callback();}};
            auto popup=builder.popover(parent,id,popup_options(input.configuration,*initial),{.opened=std::move(opened),.closed=std::move(closed),.canceled=std::move(canceled)});
            if(!popup)return std::unexpected(popup.error());state->root=*popup;
            auto content=builder.vstack(*popup,std::string(id)+"/content",layout(input.configuration));if(!content)return std::unexpected(content.error());state->content=*content;
            if(auto r=content->set_accessibility_semantics(semantics(input.configuration));!r)return std::unexpected(r.error());
            if(auto r=content->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            if(auto r=content->set_input_handler([state](const auto& value){return NativeMenu::input(state,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto value=weak.lock())value->live=false;});!r)return std::unexpected(r.error());
            return NativeMenu(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget content_root()const{return state_->content;}
    [[nodiscard]] ii::core::Result<MenuInput> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());try{return state_->authored;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<int> selected_index()const{if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->model.selected_index();}
    [[nodiscard]] ii::core::Result<void> status()const{if(auto valid=check(state_);!valid)return valid;if(state_->failure)return std::unexpected(*state_->failure);return {};}
    [[nodiscard]] ii::core::Result<void> set_options(MenuInput value)const{
        if(auto valid=check(state_);!valid)return valid;
        if(!Menu::validate(value.configuration)||value.selected_index < -1||value.selected_index>=4096)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Menu configuration");
        try{
            if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
            auto previous=state_->authored;auto old=state_->model;
            if(auto configured=state_->model.set_configuration(value.configuration);!configured)return std::unexpected(convert(configured.error()));
            if(state_->finalized&&value.selected_index!=previous.selected_index){auto selected=state_->model.select(value.selected_index);
                if(!selected){state_->model=std::move(old);return std::unexpected(convert(selected.error()));}}
            const auto revision=++state_->revision;state_->authored=std::move(value);
            if(auto result=sync(state_);!result){if(state_->revision==revision){state_->authored=std::move(previous);state_->model=std::move(old);(void)sync(state_);}return result;}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> register_item(MenuItemPort port)const{
        if(auto valid=check(state_);!valid)return valid;
        if(!port.present||!port.activated||!port.changed||!port.entry||!port.snapshot||!port.focus||!port.open_submenu||!port.parent_close||!port.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete menu item port");
        bool action_attached=false,observer_attached=false,owner_attached=false;
        try{
            if(state_->entries.size()>=4096||state_->next_id==std::numeric_limits<std::uint64_t>::max()||state_->revision==std::numeric_limits<std::uint64_t>::max())
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu item or revision limit exceeded");
            auto item=port.snapshot(),parent=state_->content.snapshot();if(!item)return std::unexpected(item.error());if(!parent)return std::unexpected(parent.error());
            if(item->parent_id!=parent->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu currently accepts direct item children only");
            if(std::ranges::any_of(state_->entries,[&](const auto& entry){return entry.widget_id==item->id;}))return ii::core::fail(ii::core::ErrorCode::already_exists,"Menu item already registered");
            const auto id=++state_->next_id;std::weak_ptr<State> weak=state_;state_->entries.reserve(state_->entries.size()+1);
            State::Entry entry{id,item->id,"item-"+std::to_string(id),port,{}};
            if(auto attached=port.parent_close([weak]()->ii::core::Result<MenuCloseAction>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return close_plan(state);});!attached)return attached;owner_attached=true;
            if(auto attached=port.activated([weak,id](std::function<void()> callback){activated(weak,id,std::move(callback));});!attached){(void)port.parent_close({});return attached;}action_attached=true;
            if(auto attached=port.changed([weak]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                    if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Menu revision exhausted");
                    ++state->revision;auto result=sync(state);if(result)state->failure.reset();else state->failure=result.error();return result;});!attached){(void)port.activated({});(void)port.parent_close({});return attached;}
            observer_attached=true;++state_->revision;state_->entries.push_back(std::move(entry));
            if(auto result=sync(state_);!result){std::erase_if(state_->entries,[&](const auto& value){return value.id==id;});(void)port.activated({});(void)port.changed({});(void)port.parent_close({});(void)sync(state_);return result;}
            return {};
        }catch(...){auto cause=std::current_exception();try{if(observer_attached)(void)port.changed({});if(action_attached)(void)port.activated({});if(owner_attached)(void)port.parent_close({});}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"Menu registration failed",cause);}
    }
    [[nodiscard]] ii::core::Result<void> register_divider(DividerGroupPort divider)const{
        if(!divider.options||!divider.visible||!divider.changed||!divider.snapshot||!divider.retired)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Incomplete menu divider port");
        MenuItemPort port{
            .present=[](MenuItemPresentation)->ii::core::Result<void>{return {};},
            .activated=[](MenuItemActivation)->ii::core::Result<void>{return {};},
            .changed=divider.changed,
            .entry=[divider]()->ii::core::Result<MenuEntry>{
                auto configuration=divider.options();if(!configuration)return std::unexpected(configuration.error());
                auto visible=divider.visible();if(!visible)return std::unexpected(visible.error());
                auto snapshot=divider.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                Divider model;if(!model.set_configuration(*configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu divider");
                return MenuEntry{.key=snapshot->id,.kind=MenuEntryKind::divider,.enabled=false,.visible=*visible,.height=model.extent().height};},
            .snapshot=divider.snapshot,
            .focus=[]()->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Menu divider cannot receive focus");},
            .open_submenu=[]()->ii::core::Result<bool>{return false;},
            .parent_close=[](MenuClosePlan)->ii::core::Result<void>{return {};},
            .retired=divider.retired
        };
        return register_item(std::move(port));
    }
    [[nodiscard]] MenuPopupPort weak_popup_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .snapshot=[weak]()->ii::core::Result<ii::gui::WidgetSnapshot>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");return state->root.snapshot();},
            .open=[weak](ii::gui::Widget anchor,MenuDirection direction,MenuCloseAction parent)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
                if(auto valid=check(state);!valid)return valid;if(!state->live||!state->finalized)return ii::core::fail(ii::core::ErrorCode::closed,"Menu is not ready");
                auto owner=anchor.snapshot(),popup=state->root.snapshot();if(!owner)return std::unexpected(owner.error());if(!popup)return std::unexpected(popup.error());
                if(popup->parent_id!=owner->id)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Submenu anchor does not own the popup");
                auto extent=state->model.layout();if(!extent)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Invalid submenu extent");
                double x=0,y=0;switch(direction){case MenuDirection::right:x=owner->bounds.width;break;case MenuDirection::left:x=-extent->width;break;
                    case MenuDirection::up:y=-extent->height;break;case MenuDirection::down:y=owner->bounds.height;break;
                    default:return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid submenu direction");}
                auto old=state->parent_close;const bool previous=state->subordinate;state->parent_close=std::move(parent);state->subordinate=true;
                auto opened=state->root.open_for(anchor,x,y);if(!opened){state->parent_close=std::move(old);state->subordinate=previous;}return opened;
            }
        };
    }
    [[nodiscard]] ii::core::Result<void> finalize()const{
        if(auto valid=check(state_);!valid)return valid;
        if(auto result=sync(state_);!result)return result;
        auto previous=state_->model;
        if(state_->authored.selected_index>=0){auto selected=state_->model.select(state_->authored.selected_index);if(!selected)return std::unexpected(convert(selected.error()));}
        if(auto result=sync(state_);!result){state_->model=std::move(previous);(void)sync(state_);return result;}
        state_->finalized=true;return {};
    }
    [[nodiscard]] MenuRegistrationPort weak_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](MenuItemPort item)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Menu has retired");
            return NativeMenu(std::move(state)).register_item(std::move(item));};
    }
};
} // namespace lvui::native
