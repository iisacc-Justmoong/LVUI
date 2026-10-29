#include "NativeOwnedTabBarSlots.generated.h"

// Keep the generated factory in the linked consumer without opening a window.
auto* volatile tabbar_factory = &lvui_generated::NativeOwnedTabBarSlots::build;

int main() {
    return tabbar_factory == nullptr;
}
