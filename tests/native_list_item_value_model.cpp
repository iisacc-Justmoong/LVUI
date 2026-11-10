#include <LVUI/NativeListItemDrawing.h>
#include <iiGUI/ListItemModel.h>
using ListItemModel=lvui::native::ListItemModelBinding<ii::gui::BasicListItem>;
#include <stdexcept>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
    lvui::ListItemConfiguration c;c.variant=lvui::ListItemVariant::navigation;c.label="Storage";c.value="512 GB";c.show_chevron=true;
    ListItemModel item;check(item.set_configuration(c),"Value configuration rejected");auto a=item.geometry(280,44);
    check(a&&a->value.width==72&&a->value.x>=a->label.x+a->label.width&&a->value.x+a->value.width<=a->chevron.x,
          "Figma value slot did not reserve distinct geometry");
    for(int width=0;width<=280;++width){auto g=item.geometry(width,44);check(g&&g->value.x>=0&&g->value.x+g->value.width<=width&&
        g->label.x+g->label.width<=g->value.x+1e-9,"Value and label overlap at narrow widths");}
    c.show_value=false;check(item.set_configuration(c),"Hidden value rejected");auto hidden=item.geometry(280,44);
    check(hidden&&hidden->value.width==0&&hidden->label.width>a->label.width,"Hidden value still reserved space");
    c.value=std::string("bad\0value",9);check(!item.set_configuration(c),"Invalid value text was accepted");
}
