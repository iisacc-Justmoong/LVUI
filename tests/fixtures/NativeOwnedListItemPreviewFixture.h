#pragma once
#include <LVUI/ListItem.h>
#include <LVUI/ListItemSelector.h>
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct NativeOwnedListItemPreviewModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::resource};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> preview{make_image()};
    ii::core::Observable<bool> show_preview{true};
    ii::core::Observable<int> version{0};
    ii::core::Observable<std::vector<std::string>> versions{{"Original","Edited"}};
    std::vector<std::string> events;
    static std::shared_ptr<const ii::gui::RasterImage> make_image(){
        auto value=ii::gui::rgba_image(2,1,{255,0,0,255,0,255,0,255});
        if(!value)throw std::runtime_error(value.error().message);return *value;
    }
    void menu(int index,const lvui::ListItemOption& item){events.push_back(std::to_string(index)+":"+item.text);}
    void versionChanged(int value){version.set(value);events.push_back("version:"+std::to_string(value));}
    void actionValues(std::string name,const lvui::ListItemActionValues&){events.push_back("values:"+name);}
    void actionEvent(std::string name,const lvui::ListItemActionPayload& payload){
        if(const auto* item=std::get_if<lvui::ListItemMenuSelection>(&payload))
            events.push_back("event:"+name+":"+std::to_string(item->index)+":"+item->item.text);
        else events.push_back("event:"+name);
    }
};
