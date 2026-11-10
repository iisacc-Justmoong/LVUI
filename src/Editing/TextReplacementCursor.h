#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace lvui {
// `offset` follows native text-service conventions: 1 is the insertion end,
// 0 is its beginning, and other values may point outside the replacement.
[[nodiscard]] inline std::size_t text_replacement_cursor(std::size_t start,std::size_t end,
                                                         std::size_t length,std::int32_t offset) noexcept {
    start=(std::min)(start,length);end=(std::min)((std::max)(start,end),length);
    if(offset>0){
        const auto delta=static_cast<std::uint64_t>(offset)-1;
        return delta>=length-end?length:end+static_cast<std::size_t>(delta);
    }
    const auto delta=static_cast<std::uint64_t>(-static_cast<std::int64_t>(offset));
    return delta>=start?0:start-static_cast<std::size_t>(delta);
}
} // namespace lvui
