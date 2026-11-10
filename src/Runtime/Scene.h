#pragma once

#include <string_view>

namespace lvui::native {

// Immutable application declaration metadata; it is not a live window handle.
struct SceneDescriptor final {
    std::string_view id;
    bool initially_open = true;
    friend constexpr bool operator==(const SceneDescriptor&, const SceneDescriptor&) = default;
};

} // namespace lvui::native
