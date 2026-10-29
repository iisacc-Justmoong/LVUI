#include <LVUI/Compiler.h>
#include <stdexcept>

namespace {void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}}
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { PanelMaterial(width: vm.width, primaryColor: vm.color, density: .Glass25, backdrop: .PreviousLayers) { Label(text: \"child\") }\n WindowMaterial()\n MaterialSurface() } }");
    if(!code)throw std::runtime_error(code.error().message);
    require(code->find("#include <LVUI/NativeMaterial.h>")!=code->npos&&code->find("lvui::native::NativeMaterial::create(")!=code->npos&&
        code->find("lvui::MaterialKind::panel")!=code->npos&&code->find("lvui::MaterialKind::window")!=code->npos&&
        code->find("component.set_options(value)")!=code->npos&&code->find("lvrs::")==code->npos&&code->find("<LVRS/")==code->npos,
        "Material code does not own its renderer and reactive input");
    for(auto text:{"Window { MaterialSurface(density: .Unknown) }","Window { PanelMaterial(tintOpacity: 2) }",
                   "Window { WindowMaterial(blurRadius: 65) }","Window { MaterialSurface(width: -1) }",
                   "Window { MaterialSurface(outlinePath: \"M0\") }"})
        require(!lvui::compile_native(text),"Unsupported or invalid material input was accepted");
    auto outline=lvui::compile_native("Window { MaterialSurface(outlinePath: \"M0 0L2 2Z\") }");
    require(outline&&outline->find("value.outline_path")!=outline->npos,"Static SVG outline was not generated");
}
