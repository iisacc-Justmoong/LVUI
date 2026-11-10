#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack {\n Tooltip(id: \"hint\", targetId: \"anchor\", text: vm.text, delay: 0, timeout: 0, hideDelay: 0, preferredPlacement: .Above, maximumWidth: 300, contentPadding: 10, onOpened: vm.opened)\n PushButton(id: \"anchor\", text: \"Anchor\")\n } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeTooltip.h>","detail_tooltip::NativeTooltip::create(","->set_target(","->content_root()","tooltip.refresh()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Tooltip lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Tooltip refers to legacy code");
    auto inline_content=lvui::compile_native("Window { VStack {\n Tooltip(targetId: \"anchor\") { Label(text: \"Help\") }\n PushButton(id: \"anchor\")\n } }");
    if(!inline_content||inline_content->find("->content_root()")==inline_content->npos)throw std::runtime_error("Inline Tooltip content was not generated");
    auto reactive=lvui::compile_native("import vm Model\nWindow { VStack {\n"
        " Tooltip(targetId: \"anchor\", maximumWidth: vm.width, maximumHeight: vm.height, contentPadding: vm.padding, "
        "edgeMargin: vm.margin, cornerRadius: vm.radius, tailLength: vm.length, tailWidth: vm.tailWidth, "
        "anchorX: vm.anchorX, anchorY: vm.anchorY, delay: vm.delay, timeout: vm.timeout, hideDelay: vm.hideDelay, "
        "automatic: vm.automatic, scrollContent: vm.scroll, motionEnabled: vm.motionEnabled, animationDuration: vm.animationDuration, motion: vm.motion, preferredPlacement: vm.placement, surfaceColor: vm.surface, borderColor: vm.border, "
        "primaryColor: vm.primary, surfaceOpacity: vm.opacity, backdrop: vm.backdrop, accessibilityName: vm.name)\n"
        " PushButton(id: \"anchor\")\n } }");
    if(!reactive)throw std::runtime_error(std::string("Reactive Tooltip options were rejected: ")+reactive.error().message);
    for(auto token:{"detail_tooltip::Input", "component.set_options(value)", "context.read(", "tooltip_duration(", "diagnostic_source()", "value.scroll_content", "value.motion_enabled", "value.motion"})
        if(reactive->find(token)==reactive->npos)throw std::runtime_error(std::string("Reactive Tooltip lost ")+token);
    const auto padding=reactive->find("scope->backend_0.padding");
    if(padding==reactive->npos||reactive->find("scope->backend_0.padding",padding+1)!=reactive->npos)
        throw std::runtime_error("Reactive Tooltip evaluated shared content padding more than once");
    auto direct=lvui::compile_native("import vm Model\nWindow { Tooltip(target: vm.target, availableRect: vm.bounds, text: \"Help\") }");
    if(!direct)throw std::runtime_error(std::string("Typed Tooltip target or available rectangle was rejected: ")+direct.error().message);
    for(auto token:{"component.set_target(","tooltip_available_rect(","context.read("})
        if(direct->find(token)==direct->npos)throw std::runtime_error(std::string("Typed Tooltip input lost ")+token);
    if(!lvui::compile_native("Window { Tooltip(maximumWidth: 0.5, scrollContent: false) }"))
        throw std::runtime_error("A valid fractional Tooltip maximum width was rejected");
    if(!lvui::compile_native("Window { Tooltip(motionEnabled: true, animationDuration: 420) }"))
        throw std::runtime_error("Valid Tooltip motion settings were rejected");
    for(auto source:{"Window { Tooltip(delay: -1) }","Window { Tooltip(preferredPlacement: .Bad) }",
                     "Window { Tooltip(delay: 1.5) }","Window { Tooltip(maximumWidth: 0) }",
                     "Window { Tooltip(scrollContent: 1) }","Window { Tooltip(motionEnabled: 1) }",
                     "Window { Tooltip(animationDuration: -1) }","Window { Tooltip(animationDuration: 1.5) }",
                     "Window { Tooltip(target: 1) }","Window { Tooltip(availableRect: 1) }",
                     "import vm Model\nWindow { Tooltip(targetId: \"x\", target: vm.target) }",
                     "Window { Tooltip(text: \"Help\") { Label(text: \"Other\") } }",
                     "import vm Model\nWindow { Tooltip(targetId: vm.target) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Unsupported Tooltip input was accepted");
}
