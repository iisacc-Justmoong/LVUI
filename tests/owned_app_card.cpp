#include <LVUI/AppCard.h>
#include <type_traits>

// The declarative shape remains available to existing C++ view models without
// importing the graphics framework or owning widget behavior in LVUI.
static_assert(std::is_enum_v<lvui::AppCardShape>);
static_assert(lvui::AppCardShape::round_rect != lvui::AppCardShape::cylinder);
int main() {}
