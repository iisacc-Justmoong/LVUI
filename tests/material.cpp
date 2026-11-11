#include <LVUI/Compiler.h>
#include <stdexcept>

namespace {void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}}
int main(){
    auto neutral=lvui::compile_native("import vm Model\nWindow(accent: vm.accent) { WindowMaterial() }");
    require(bool(neutral),"Neutral window material did not compile");
    const auto window_kind=neutral->find("value.kind = lvui::MaterialKind::window");
    const auto window_end=neutral->find("return value;",window_kind);
    require(window_kind!=neutral->npos&&window_end!=neutral->npos&&
        neutral->substr(window_kind,window_end-window_kind).find("value.primary_color")==neutral->npos,
        "Window material unexpectedly inherited the application accent");
    lvui::NativeCompileOptions theme;
    theme.style_tokens.overrides["--lvui-theme-colors-panel-window"]=lvui::StyleColor{.25,.5,.75,1};
    auto themed=lvui::compile_native("Window { WindowMaterial() }",theme);
    require(themed&&themed->find("value.color = lvui::StyleColor{0.25, 0.5, 0.75, 1};")!=themed->npos,
        "Window material ignored the resolved Panel/window token");
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { PanelMaterial(width: vm.width, primaryColor: vm.color, density: .Glass25, backdrop: .PreviousLayers) { Label(text: \"child\") }\n WindowMaterial()\n MaterialSurface() } }");
    if(!code)throw std::runtime_error(code.error().message);
    require(code->find("#include <LVUI/NativeMaterial.h>")!=code->npos&&code->find("lvui::native::MaterialBinding<")!=code->npos&&
        code->find("lvui::MaterialKind::panel")!=code->npos&&code->find("lvui::MaterialKind::window")!=code->npos&&
        code->find("component.set_options(value)")!=code->npos&&code->find("lvrs::")==code->npos&&code->find("<LVRS/")==code->npos,
        "Material code does not own its renderer and reactive input");
    for(auto text:{"Window { MaterialSurface(density: .Unknown) }","Window { PanelMaterial(tintOpacity: 2) }",
                   "Window { WindowMaterial(blurRadius: 65) }","Window { MaterialSurface(width: -1) }",
                   "Window { MaterialSurface(outlinePath: \"M0\") }"})
        require(!lvui::compile_native(text),"Unsupported or invalid material input was accepted");
    auto outline=lvui::compile_native("Window { MaterialSurface(outlinePath: \"M0 0L2 2Z\") }");
    require(outline&&outline->find("value.outline_path")!=outline->npos,"Static SVG outline was not generated");
    auto sources=lvui::compile_native("import vm Model\nWindow { VStack { PanelMaterial(backdropBackground: \"background\", backdropSource: vm.source)\n Label(id: \"background\") } }");
    require(sources&&sources->find("lvui::native::MaterialSourcesValue<ii::gui::Widget>{")!=sources->npos&&
        sources->find("material_background = *node_")!=sources->npos&&sources->find("component.set_sources(value.background, value.content)")!=sources->npos,
        "Mixed forward ID and reactive material sources were not generated");
    auto fixed=lvui::compile_native("Window { VStack { PanelMaterial(backdropBackground: \"a\", backdropSource: \"b\")\n Label(id: \"a\")\n Label(id: \"b\") } }");
    require(fixed&&fixed->find("->set_sources(*node_")!=fixed->npos,"Static material source IDs were not bound");
    auto scoped=lvui::compile_native("import vm Model\nWindow { if (vm.ready) { VStack { PanelMaterial(backdropSource: \"local\")\n Label(id: \"local\") } } }");
    require(scoped&&scoped->find("->set_sources(")!=scoped->npos,"Material source was not bound inside conditional content");
    for(auto text:{"Window { PanelMaterial(backdropSource: \"missing\") }", "Window { PanelMaterial(backdropBackground: 3) }",
                   "import vm Model\nWindow { VStack { if (vm.ready) { Label(id: \"local\") }\n PanelMaterial(backdropSource: \"local\") } }"})
        require(!lvui::compile_native(text),"Invalid or out-of-scope material source was accepted");
}
