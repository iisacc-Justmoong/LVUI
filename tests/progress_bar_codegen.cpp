#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("LVUI ProgressBar generation failed"); }
}

int main() {
    const auto code = lvui::compile_native(
        "import vm Backend\nWindow { ProgressBar(minimumValue: 0, maximumValue: 100, "
        "startValue: 20, currentValue: vm.value) }");
    require(bool(code));
    for (std::string_view expected : {"#include <LVUI/ProgressBar.h>", "widgets.absolute_stack(",
                                      "std::make_shared<lvui::ProgressBar>()", "set_paint_handler(",
                                      "->geometry(paint.size.width, paint.size.height)",
                                      "->set_animated_configuration(value", "target.request_repaint()",
                                      "set_accessibility_semantics(", "AccessibilityRole::progress"})
        require(code->find(expected) != code->npos);
    for (std::string_view forbidden : {"#include <LVRS/", "lvrs::", "lvrs_appearance()", "widgets.progress("})
        require(code->find(forbidden) == code->npos);
    require(bool(lvui::compile_native("Window { ProgressBar(minimumValue: 5, maximumValue: 2) }")));
    require(!lvui::compile_native("Window { ProgressBar(width: 0) }"));
    require(!lvui::compile_native("Window { ProgressBar(maximumValue: 10, endValue: 20) }"));
    require(!lvui::compile_native("Window { ProgressBar(size: .unknown) }"));
    require(!lvui::compile_native("Window { ProgressBar(trackColor: \"bad\") }"));
    const auto advanced = lvui::compile_native(
        "import vm Backend\nWindow { ProgressBar(minimumValue: vm.minimum, endValue: vm.maximum, "
        "startValue: vm.start, currentValue: vm.current, width: vm.width, height: vm.height, "
        "size: vm.size, shapeStyle: vm.shape, regularHeight: vm.regular, cornerRadius: vm.radius, "
        "trackColor: vm.track, fillColor: vm.fill, displayedStart: vm.displayedStart, "
        "displayedProgress: vm.displayedProgress) }");
    require(bool(advanced));
    for (std::string_view expected : {"lvui::ProgressConfiguration", "native_bindings->derive(",
                                      "lvui::progress_size(", "lvui::progress_shape(",
                                      "lvui::native::color<lvui::StyleColor>", "target.set_extent(",
                                      "->set_animated_configuration(value"})
        require(advanced->find(expected) != advanced->npos);
    for (std::string_view forbidden : {"#include <LVRS/", "lvrs::", "lvrs_appearance()"})
        require(advanced->find(forbidden) == advanced->npos);
    const auto sourced = lvui::compile_native(
        "import vm Backend\nWindow { ProgressBar(stateModel: vm.state, "
        "currentValueStateKey: vm.key, minimumValueStateKey: \" lo \", currentValue: vm.fallback) }");
    require(bool(sourced));
    for (std::string_view expected : {"lvui::ProgressStateKeys", "lvui::progress_values(",
                                      "lvui::native::text(", "native_bindings->derive("})
        require(sourced->find(expected) != sourced->npos);
    for (std::string_view forbidden : {"#include <LVRS/", "lvrs::"})
        require(sourced->find(forbidden) == sourced->npos);
    const auto imported = lvui::compile_native(
        "import state lvui.ProgressState\nWindow { ProgressBar(stateModel: state) }");
    require(imported && imported->find("lvui::progress_values(") != imported->npos);
    require(!lvui::compile_native("Window { ProgressBar(stateModel: 4) }"));
    require(!lvui::compile_native("Window { ProgressBar(currentValueStateKey: \"count\") }"));
    const auto motion = lvui::compile_native(
        "import vm Backend\nWindow { ProgressBar(currentValue: vm.value, motionEnabled: vm.enabled, motion: vm.motion) }");
    require(bool(motion));
    for (std::string_view expected : {"lvui::motion_value(", "set_animated_configuration(",
                                      "set_frame_handler(", "request_frames()"})
        require(motion->find(expected) != motion->npos);
    for (std::string_view forbidden : {"#include <LVRS/", "lvrs::"})
        require(motion->find(forbidden) == motion->npos);
    require(!lvui::compile_native("Window { ProgressBar(motion: 1) }"));
    require(!lvui::compile_native("Window { ProgressBar(motionEnabled: 4) }"));
}
