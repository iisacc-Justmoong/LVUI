#include <LVUI/IconCatalog.h>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    auto builtins=lvui::builtin_icon_catalog();require(builtins&&(*builtins)->size()>=12,"LVUI builtin icons were not embedded");
    for(auto name:{"generaladd","home","nodesfolder","projectStructure","procedure","generalchevronDown","generalchevronUp","generalchevronUpDown","generalchevronRight","generalchevronLeft","alertAdjustments","alertFileText","colorEyedropper","addFile","generaldelete","settings"}){
        auto asset=(*builtins)->resolve(name);require(asset&&(*asset)->svg.find("<svg")!=std::string::npos,"A built-in SVG icon is unavailable");
    }
    auto custom=lvui::IconCatalog::create({{"a","<svg/>"},{"b","<svg/>"}});
    require(custom&&(*custom)->size()==2&&(*custom)->revision()==1,"Custom icon catalog was not created");
    auto first=(*custom)->resolve("a"),again=(*custom)->resolve("a");
    require(first&&again&&*first==*again&&(*first)->svg=="<svg/>","Icon lookup lost immutable asset identity");
    auto missing=(*custom)->resolve("missing");require(!missing&&missing.error().code==lvui::IconErrorCode::not_found,"Unknown icon did not return not_found");
    require(!(*custom)->resolve("")&&!(*custom)->resolve(std::string("a\0b",3)),"Invalid icon name was accepted");
    auto duplicate=lvui::IconCatalog::create({{"a","<svg/>"},{"a","<svg/>"}});
    require(!duplicate&&duplicate.error().code==lvui::IconErrorCode::duplicate,"Duplicate icon was accepted");
    require(!lvui::IconCatalog::create({{"a",std::string("<svg/>\0",7)}}),"NUL in SVG was accepted");
    auto capped=lvui::IconCatalog::create({{"a","<svg/>"},{"b","<svg/>"}},
        {.maximum_entries=2,.maximum_svg_bytes=6,.maximum_total_bytes=10});
    require(!capped&&capped.error().code==lvui::IconErrorCode::limit_exceeded,"Aggregate icon budget was not enforced");
    require(!lvui::IconCatalog::create({}, {.maximum_entries=0}),"Invalid icon limits were accepted");
}
