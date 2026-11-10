#pragma once

namespace lvui::native {
// Older iiGUI packages report pointer cancellation as focus loss. Newer ones
// distinguish it, so a gesture can stop without dismissing the active editor.
template<class Kind>
[[nodiscard]] constexpr bool pointer_canceled(Kind kind) noexcept {
    if (kind == Kind::focus_lost) return true;
    if constexpr (requires { Kind::pointer_cancel; })
        if (kind == Kind::pointer_cancel) return true;
    return false;
}
} // namespace lvui::native
