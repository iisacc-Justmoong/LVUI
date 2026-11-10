#include <LVUI/Material.h>
#include <stdexcept>
int main(){
    lvui::MaterialConfiguration value;value.kind=lvui::MaterialKind::panel;
    if(!lvui::MaterialModelPolicy::valid_extra(value)||!lvui::valid_material_outline("M0 0L10 10Z")||lvui::valid_material_outline("M0"))
        throw std::runtime_error("Material authoring contract changed");
}
