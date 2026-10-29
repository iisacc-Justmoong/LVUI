#include "NativeWindowAccent.generated.h"

// Link the generated factory and public LVUI accent type without opening a window.
auto* volatile window_factory = &lvui_generated::NativeWindowAccent::build;

int main() {
    return window_factory == nullptr;
}
