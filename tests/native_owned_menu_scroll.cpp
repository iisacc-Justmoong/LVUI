#include "native_menu_composition.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,180}));std::optional<native::NativeMenu> menu,child;
    std::vector<native::NativeMenuItem> items;std::vector<int> activated;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto popup=native::NativeMenu::create(builder,*stack,"menu",{}, {.triggered=[&](int index){activated.push_back(index);}});
        if(!popup)return std::unexpected(popup.error());menu=*popup;
        for(int i=0;i<30;++i){native::MenuItemInput input;input.configuration.label="Option "+std::to_string(i);
            input.configuration.icon_name.clear();input.configuration.show_icon_slot=false;input.configuration.show_chevron=false;input.configuration.has_children=i==3;
            input.configuration.enabled=i!=4;
            auto item=native::NativeMenuItem::create(builder,menu->content_root(),"option-"+std::to_string(i),input);
            if(!item)return std::unexpected(item.error());if(auto registered=menu->register_item(item->weak_group_port());!registered)return registered;
            if(i==3){auto nested=native::NativeMenu::create(builder,item->root(),"nested");if(!nested)return std::unexpected(nested.error());child=*nested;
                if(auto finalized=child->finalize();!finalized)return finalized;
                if(auto attached=item->set_submenu(child->weak_popup_port());!attached)return attached;}
            items.push_back(*item);
        }
        return menu->finalize();
    }));
    good(menu->root().open_at(10,10));auto popup=take(menu->root().snapshot());
    require(popup.content_height>popup.bounds.height,"Long menu did not exceed its viewport");
    good(items.back().root().focus());popup=take(menu->root().snapshot());
    require(popup.scroll_offset>0,"Keyboard focus did not reveal an offscreen menu item");
    good(items.back().root().activate());require(activated==std::vector<int>{29}&&!take(menu->root().snapshot()).opened,
            "Revealed menu item was not activated and closed");
    good(menu->root().open_at(10,10));popup=take(menu->root().snapshot());
    good(tree->input({.kind=WidgetInputKind::scroll,.x=popup.bounds.x+10,.y=popup.bounds.y+10,.scroll_y=60}));
    require(take(menu->root().snapshot()).scroll_offset>0,"Pointer scrolling did not move the menu viewport");
    const auto third=take(items[3].root().snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=third.x+third.width/2,.y=third.y+third.height/2}));
    require(take(menu->selected_index())==3&&take(child->root().snapshot()).opened&&activated==std::vector<int>{29},
            "Pointer selection in a scrolled menu targeted the wrong row, failed to open its submenu, or activated it");
    const auto disabled=take(items[4].root().snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=disabled.x+disabled.width/2,.y=disabled.y+disabled.height/2}));
    require(take(menu->selected_index())==3&&!take(child->root().snapshot()).opened&&activated==std::vector<int>{29},
            "A disabled row in a scrolled menu was selected, activated, or failed to dismiss its sibling submenu");
    good(host.clear_content());tree->close();
}
