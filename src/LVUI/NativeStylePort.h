#pragma once
#include <iiGUI/NativeWidgets.h>
#include <functional>
namespace lvui::native {
// A replaceable, non-owning connection to a child's temporary visual policy.
// retired is an optional owner-thread query; applying a retired port returns closed.
struct BorderlessTonePort final {
    std::function<ii::core::Result<void>(bool)> apply;
    std::function<bool()> retired;
};
} // namespace lvui::native
