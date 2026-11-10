#include <LVUI/Compiler.h>
#include <stdexcept>
int main(){auto value=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Detail, label: vm.label, detail: vm.detail, showLeadingIcon: false, showBookmark: false, showDate: vm.show, showFolders: false, showTags: false, dateText: vm.date) }");
    if(!value)throw std::runtime_error(value.error().message);
    for(auto text:{"configuration.show_detail_date","configuration.show_detail_folders","configuration.show_detail_tags","configuration.detail_date"})
        if(value->find(text)==value->npos)throw std::runtime_error("Detailed file metadata input missing");}
