#include <LVUI/Menu.h>
#include <stdexcept>
int main(){
    lvui::MenuConfiguration config;lvui::MenuEntry entry;entry.key="item";
    if(config.item_width!=141||entry.kind!=lvui::MenuEntryKind::action||!lvui::MenuModelPolicy::valid_text(entry.key)||
       lvui::MenuModelPolicy::valid_text(std::string(1,'\0')))
        throw std::runtime_error("Menu value or text contract changed");
}
