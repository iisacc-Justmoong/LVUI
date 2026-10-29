#pragma once
// Optional generated-view adapter. Core navigation/measurement headers remain
// independent of iiGUI; a widget consumer explicitly links its native UI host.
#include <LVUI/NativeCollectionNavigation.h>
#include <iiGUI/NativeWidgets.h>
#include <set>

namespace lvui::widgets {
struct CollectionNavigationEvents final {
    std::function<void(std::string)> current_changed,activated;
    std::function<void(std::vector<std::string>)> selection_changed;
};
template<class T,class Navigation> class CollectionWidgetNavigation final:
    public std::enable_shared_from_this<CollectionWidgetNavigation<T,Navigation>> {
public:
    using Frame=lvui::native::VirtualCollectionFrame<T>;
    using Admission=std::function<std::shared_ptr<void>()>;
    using Filter=std::function<bool(const T&,std::size_t)>;
    static ii::core::Result<std::shared_ptr<CollectionWidgetNavigation>> create(
        ii::gui::WidgetBuilder& widgets,ii::gui::Widget target,Navigation& navigation,
        std::shared_ptr<lvui::native::VirtualViewport> viewport,Admission admission,
        std::function<void()> publish_revision,CollectionNavigationEvents events={},Filter filter={}){
        auto value=std::shared_ptr<CollectionWidgetNavigation>(new CollectionWidgetNavigation(target,navigation,std::move(viewport),std::move(admission),std::move(publish_revision),std::move(events),std::move(filter)));
        auto weak=std::weak_ptr(value);
        if(auto retained=widgets.retain(value);!retained)return std::unexpected(retained.error());
        if(auto retiring=widgets.on_retire([weak]{if(auto owner=weak.lock())owner->close();});!retiring)return std::unexpected(retiring.error());
        if(auto input=target.set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{if(auto owner=weak.lock())return owner->input(event);return false;});!input)return std::unexpected(input.error());
        if(auto escape=target.set_escape_action([weak]{if(auto owner=weak.lock())owner->escape();});!escape)return std::unexpected(escape.error());
        value->watch_=navigation.state.observe([weak](const auto& state,std::uint64_t){if(auto owner=weak.lock())owner->changed(state);});
        if(!value->last_.current_key.empty())value->viewport_->reveal(value->last_.current_key);
        return value;
    }
    void publish(const Frame& frame){owner();if(active_)frame_=std::make_shared<const Frame>(frame);}
    void select_on_click(bool enabled){owner();select_on_click_=enabled;}
    void close(){owner();active_=false;watch_.reset();frame_.reset();viewport_.reset();}
private:
    CollectionWidgetNavigation(ii::gui::Widget target,Navigation& navigation,
        std::shared_ptr<lvui::native::VirtualViewport> viewport,Admission admission,std::function<void()> revision,
        CollectionNavigationEvents events,Filter filter):target_(target),navigation_(&navigation),viewport_(std::move(viewport)),
        admission_(std::move(admission)),revision_(std::move(revision)),events_(std::move(events)),filter_(std::move(filter)),last_(navigation.state.get()){}
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection widget navigation requires its owner thread");}
    void changed(const lvui::native::CollectionSelectionState& state){
        owner();if(!active_)return;auto admitted=admission_();if(!admitted)return;
        const bool focus=last_.current_key!=state.current_key,selection=last_.selected_keys!=state.selected_keys;last_=state;
        if(focus)viewport_->reveal(state.current_key);
        if(!active_||!admission_())return;
        if((focus||selection)&&revision_)revision_();
        if(!active_||!admission_())return;
        if(focus&&events_.current_changed)events_.current_changed(state.current_key);
        if(active_&&admission_()&&selection&&events_.selection_changed)events_.selection_changed(state.selected_keys);
    }
    ii::core::Result<bool> input(const ii::gui::WidgetInput& event){
        owner();if(!active_)return false;auto admitted=admission_();if(!admitted||!frame_||!frame_->source)return false;
        if(event.kind==ii::gui::WidgetInputKind::key_down&&event.key==ii::gui::WidgetKey::escape)escape_allowed_=!event.text_input_active&&!event.native_navigation_active;
        auto frame=frame_;
        const auto eligible=[&](const T& value,std::size_t index){return !filter_||filter_(value,index);};
        const auto modifiers=lvui::native::CollectionModifiers{.range=event.shift,.additive=event.control||event.meta};
        lvui::native::CollectionNavigationReply reply;
        if(event.kind==ii::gui::WidgetInputKind::pointer_down){
            if(event.native_control_active||(event.buttons&~1u))return false;
            auto snapshot=target_.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            if(!active_||!admission_()||!frame_)return true;frame=frame_;
            const auto x=event.x-snapshot->bounds.x,y=event.y-snapshot->bounds.y;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                if(cell.width<=0||cell.height<=0||x<cell.x||y<cell.y||x>=cell.x+cell.width||y>=cell.y+cell.height)continue;
                const bool select=select_on_click_||modifiers.additive||modifiers.range;
                reply=navigation_->focus_at(*frame->source,frame->collection.items[n].index,modifiers,select,eligible);break;
            }
        }else if(event.kind==ii::gui::WidgetInputKind::key_down){
            if(event.text_input_active||event.native_navigation_active||event.alt)return false;
            using Key=ii::gui::WidgetKey;using Command=lvui::native::CollectionCommand;
            std::optional<Command> command;
            switch(event.key){
            case Key::left:command=Command::left;break;case Key::right:command=Command::right;break;
            case Key::up:command=Command::up;break;case Key::down:command=Command::down;break;
            case Key::home:command=Command::home;break;case Key::end:command=Command::end;break;
            case Key::page_up:command=Command::page_previous;break;case Key::page_down:command=Command::page_next;break;
            case Key::select_all:if(event.control||event.meta)command=Command::select_all;break;
            case Key::space:if(!event.native_control_active)command=Command::toggle;break;
            case Key::enter:if(!event.native_control_active)command=Command::activate;break;
            case Key::escape:if(!navigation_->state.get().selected_keys.empty())command=Command::clear;break;
            default:break;
            }
            if(!command)return false;
            std::set<std::size_t> lines;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                const bool visible=frame->layout.horizontal?cell.x+cell.width>0&&cell.x<frame->width:cell.y+cell.height>0&&cell.y<frame->height;
                if(visible)lines.insert(frame->layout.horizontal?frame->collection.items[n].index:frame->collection.items[n].index/frame->columns);
            }
            lvui::native::CollectionNavigationLayout layout{.columns=frame->layout.horizontal?1:frame->columns,.page_rows=std::max<std::size_t>(1,lines.size()),.horizontal=frame->layout.horizontal};
            if(event.native_control_active)for(const auto& item:frame->collection.items){
                auto row=target_.keyed_content_item(item.key);if(!row)continue;auto interaction=row->interaction();
                if(interaction&&interaction->focused){layout.current_index=item.index;break;}
            }
            reply=navigation_->command(*frame->source,*command,layout,modifiers,eligible);
        }else return false;
        if(!reply.handled)return false;
        if(!active_||!admission_())return true;
        const auto key=navigation_->state.get().current_key;
        if(!reply.focus_changed&&!key.empty())viewport_->reveal(key);
        if(auto focused=target_.focus();!focused)return std::unexpected(focused.error());
        if(active_&&admission_()&&reply.activated&&events_.activated)events_.activated(key);
        return true;
    }
    void escape(){
        owner();if(!active_||!escape_allowed_||!frame_||!frame_->source)return;
        auto admitted=admission_();if(!admitted||navigation_->state.get().selected_keys.empty())return;
        auto frame=frame_;(void)navigation_->command(*frame->source,lvui::native::CollectionCommand::clear);
    }
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true,select_on_click_=true,escape_allowed_=true;
    ii::gui::Widget target_;
    Navigation* navigation_;
    std::shared_ptr<lvui::native::VirtualViewport> viewport_;
    Admission admission_;
    std::function<void()> revision_;
    CollectionNavigationEvents events_;
    Filter filter_;
    lvui::native::CollectionSelectionState last_;
    ii::core::ObservableSubscription watch_;
    std::shared_ptr<const Frame> frame_;
};
} // namespace lvui::widgets
