#include <LVUI/ColorSelection.h>
int main(){
    lvui::ColorSelection color({.25,.5,.75,1});
    auto edit=color.begin();if(!edit)return 1;
    if(!color.edit(*edit,{1,0,0,1}))return 2;
    if(!color.cancel(*edit))return 3;
    auto restored=color.color();
    return !restored || *restored!=lvui::StyleColor{.25,.5,.75,1};
}
