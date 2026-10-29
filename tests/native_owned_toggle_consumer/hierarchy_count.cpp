#include "NativeHierarchyCountView.generated.h"

// Link the generated count View and its owner without opening a window.
auto* volatile hierarchy_factory = &lvui_generated::NativeHierarchyCountView::build;

int main() {
    return hierarchy_factory == nullptr;
}
