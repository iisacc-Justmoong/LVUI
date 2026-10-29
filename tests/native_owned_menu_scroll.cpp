#include <LVUI/NativeMenu.h>
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
    auto host=take(tree->content_host({},"host",{300,180}));std::optional<native::NativeMenu> menu;
    std::vector<native::NativeMenuItem> items;std::vector<int> activated;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto popup=native::NativeMenu::create(builder,*stack,"menu",{}, {.triggered=[&](int index){activated.push_back(index);}});
        if(!popup)return std::unexpected(popup.error());menu=*popup;
        for(int i=0;i<30;++i){native::MenuItemInput input;input.configuration.label="Option "+std::to_string(i);
            input.configuration.icon_name.clear();input.configuration.show_icon_slot=false;input.configuration.has_children=false;
            auto item=native::NativeMenuItem::create(builder,menu->content_root(),"option-"+std::to_string(i),input);
            if(!item)return std::unexpected(item.error());if(auto registered=menu->register_item(item->weak_group_port());!registered)return registered;
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
    good(host.clear_content());tree->close();
}
