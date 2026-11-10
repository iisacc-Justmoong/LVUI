#include <LVUI/NativeWidgetLayout.h>

#include <optional>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
enum class Alignment { center = 9, leading = 5, trailing = 2, stretch = 7 };
struct Axes { Alignment horizontal, vertical; };
struct Insets { double left, top, right, bottom; };
struct Stack {
    double spacing = 0, padding = 0;
    Alignment alignment = Alignment::center;
    std::optional<Insets> insets;
    double minimum_width = 0, minimum_height = 0;
    bool intrinsic_width = false, measure_hidden_children = false;
    std::optional<Alignment> horizontal_alignment, vertical_alignment;
};
struct Extent { double width = 0, height = 0; };
struct Layout { Stack stack; Extent extent; double flex = 0; };
}
int main() {
    using namespace lvui::native;
    require(stack_alignment(Alignment::leading, StackAxis::vertical) ==
            LayoutAlignmentAxes{LayoutAlignment::leading, LayoutAlignment::leading}, "Host alignment mapping changed");
    require(stack_alignment(Axes{Alignment::trailing, Alignment::stretch}, StackAxis::overlay) ==
            LayoutAlignmentAxes{LayoutAlignment::trailing, LayoutAlignment::stretch}, "Host axes mapping changed");
    require(stack_alignment("topLeading", StackAxis::overlay) ==
            LayoutAlignmentAxes{LayoutAlignment::leading, LayoutAlignment::leading}, "Named alignment changed");
    bool rejected = false;
    try { (void)stack_alignment(static_cast<Alignment>(99), StackAxis::vertical); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid host alignment was accepted");
    StackLayoutInput input; input.spacing = -1; input.default_spacing = 6; input.padding = 3;
    input.left_padding = 5; input.alignment = {LayoutAlignment::leading, LayoutAlignment::trailing};
    input.width = 100; input.height = 50; input.minimum_width = 40; input.flex = 1; input.intrinsic_width = true;
    auto result = stack_layout<Layout>(input);
    require(result.stack.spacing == 6 && result.stack.padding == 3 && result.stack.alignment == Alignment::center &&
            result.stack.horizontal_alignment == Alignment::leading && result.stack.vertical_alignment == Alignment::trailing &&
            result.stack.insets && result.stack.insets->left == 5 && result.stack.insets->top == 3 &&
            result.extent.width == 100 && result.extent.height == 50 && result.flex == 1 && result.stack.intrinsic_width,
            "Generic layout conversion changed the requested values");
}
