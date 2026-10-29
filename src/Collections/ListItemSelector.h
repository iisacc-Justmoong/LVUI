#pragma once

#include <LVUI/Text.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

struct ListItemOption final {
    std::string text;
    bool enabled=true;
    friend bool operator==(const ListItemOption&,const ListItemOption&)=default;
};

struct ListItemSelectorConfiguration final {
    std::vector<ListItemOption> options;
    int selected_index=0;
    std::string placeholder="Option";
    bool enabled=true;
    friend bool operator==(const ListItemSelectorConfiguration&,const ListItemSelectorConfiguration&)=default;
};

// Selection is separate from the view that opens and presents its options.
class ListItemSelector final {
public:
    [[nodiscard]] static bool validate(const ListItemSelectorConfiguration& value)noexcept{
        if(value.options.size()>400||value.selected_index<0||
           (!value.options.empty()&&std::size_t(value.selected_index)>=value.options.size())||
           (value.options.empty()&&value.selected_index!=0)||value.placeholder.size()>8192||!valid_accessibility_text(value.placeholder))return false;
        std::size_t bytes=value.placeholder.size();
        for(const auto& option:value.options){
            if(option.text.size()>8192||!valid_accessibility_text(option.text)||option.text.size()>1024*1024-bytes)return false;
            bytes+=option.text.size();
        }
        return true;
    }
    bool set_configuration(ListItemSelectorConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const ListItemSelectorConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] const std::string& display_text()const noexcept{
        return configuration_.options.empty()?configuration_.placeholder:configuration_.options[std::size_t(configuration_.selected_index)].text;
    }
    [[nodiscard]] std::optional<int> choose(int index)noexcept{
        auto& c=configuration_;if(!c.enabled||index<0||std::size_t(index)>=c.options.size()||!c.options[std::size_t(index)].enabled||index==c.selected_index)return std::nullopt;
        c.selected_index=index;return index;
    }
    [[nodiscard]] std::optional<int> navigate(int direction)noexcept{
        auto& c=configuration_;if(!c.enabled||(direction!=-1&&direction!=1)||c.options.empty())return std::nullopt;
        for(int index=c.selected_index+direction;index>=0&&std::size_t(index)<c.options.size();index+=direction)
            if(c.options[std::size_t(index)].enabled)return choose(index);
        return std::nullopt;
    }
private:
    ListItemSelectorConfiguration configuration_;
};

} // namespace lvui
