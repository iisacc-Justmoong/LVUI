#pragma once

#include <LVUI/Hierarchy.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {

enum class HierarchyReveal { contain, start, end, center };
struct HierarchyViewportAnchor final {
    std::string key;
    std::size_t index = 0;
    double within = 0;
    std::uint64_t movement = 0;
};
struct HierarchyViewportRequest final {
    double width = 0, height = 0, offset = 0, row_height = 20;
    std::size_t overscan = 1;
    std::uint64_t movement = 0;
    HierarchyViewportAnchor anchor;
    std::string reveal_key;
    HierarchyReveal reveal = HierarchyReveal::contain;
};
struct HierarchyViewportRow final { std::size_t index = 0; double y = 0, height = 0; };
class HierarchyViewportSource;
struct HierarchyViewportFrame final {
    std::shared_ptr<const HierarchyViewportSource> source;
    std::vector<HierarchyViewportRow> rows;
    double width = 0, height = 0, content_height = 0, offset = 0;
    std::uint64_t source_revision = 0, view_revision = 0, movement = 0;
    HierarchyViewportAnchor anchor;
    std::string revealed_key;
};

// Index one immutable visible projection. Scrolling reuses its key map and
// materializes only the viewport plus bounded overscan, not the whole tree.
class HierarchyViewportSource final : public std::enable_shared_from_this<HierarchyViewportSource> {
public:
    HierarchyViewportSource(const HierarchyViewportSource&) = delete;
    HierarchyViewportSource& operator=(const HierarchyViewportSource&) = delete;
    [[nodiscard]] static HierarchyResult<std::shared_ptr<const HierarchyViewportSource>> create(
            std::shared_ptr<const HierarchyProjection> projection) {
        if (!projection || projection->entries.size() > hierarchy_maximum_items)
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport projection");
        std::size_t bytes = 0;
        for (std::size_t index = 0; index < projection->entries.size(); ++index) {
            const auto& row = projection->entries[index]; const auto& entry = row.entry;
            if (entry.key.empty() || entry.key.size() > 1024 || !valid_accessibility_text(entry.key) ||
                !valid_accessibility_text(entry.parent) || !valid_accessibility_text(entry.label) || !valid_accessibility_text(entry.icon_name) ||
                entry.count < -1 || row.depth > hierarchy_maximum_items ||
                (index && row.source_index <= projection->entries[index - 1].source_index))
                return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport row");
            const auto length = entry.key.size() + entry.parent.size() + entry.label.size() + entry.icon_name.size();
            if (length > 64 * 1024 * 1024 - bytes) return error(HierarchyErrorCode::invalid, "Hierarchy viewport text exceeds its memory limit");
            bytes += length;
        }
        auto source = std::shared_ptr<HierarchyViewportSource>(new HierarchyViewportSource(std::make_shared<const HierarchyProjection>(*projection)));
        source->positions_.reserve(source->projection_->entries.size());
        for (std::size_t index = 0; index < source->projection_->entries.size(); ++index) {
            const auto& row = source->projection_->entries[index];
            if (!source->positions_.emplace(row.entry.key, index).second)
                return error(HierarchyErrorCode::invalid, "Duplicate hierarchy viewport key");
        }
        return std::shared_ptr<const HierarchyViewportSource>(std::move(source));
    }
    [[nodiscard]] const HierarchyProjection& projection() const noexcept { return *projection_; }
    [[nodiscard]] std::optional<std::size_t> find(std::string_view key) const {
        const auto found = positions_.find(key); return found == positions_.end() ? std::nullopt : std::optional(found->second);
    }
    [[nodiscard]] HierarchyResult<HierarchyViewportFrame> frame(const HierarchyViewportRequest& request) const {
        const auto bounded = [](double value, double low, double high) { return std::isfinite(value) && value >= low && value <= high; };
        if (!bounded(request.width, 0, 8192) || !bounded(request.height, 0, 8192) ||
            !bounded(request.offset, 0, 1e15) || !bounded(request.row_height, 1, 8192) ||
            !bounded(request.anchor.within, 0, 8192) || request.overscan > hierarchy_maximum_items ||
            static_cast<unsigned>(request.reveal) > static_cast<unsigned>(HierarchyReveal::center) ||
            !valid_accessibility_text(request.reveal_key) || !valid_accessibility_text(request.anchor.key))
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport request");
        HierarchyViewportFrame result; result.source = shared_from_this(); result.width = request.width; result.height = request.height;
        result.source_revision = projection_->source_revision; result.view_revision = projection_->view_revision; result.movement = request.movement;
        const auto count = projection_->entries.size(); result.content_height = count * request.row_height;
        auto offset = request.offset;
        if (!request.anchor.key.empty() && request.anchor.movement == request.movement && count) {
            const auto index = find(request.anchor.key).value_or(std::min(request.anchor.index, count - 1));
            offset = index * request.row_height + std::min(request.anchor.within, std::nextafter(request.row_height, 0.));
        }
        if (!request.reveal_key.empty() && request.width > 0 && request.height > 0) if (const auto index = find(request.reveal_key)) {
            const auto begin = *index * request.row_height, end = begin + request.row_height;
            if (request.reveal == HierarchyReveal::start || request.row_height >= request.height) offset = begin;
            else if (request.reveal == HierarchyReveal::end) offset = end - request.height;
            else if (request.reveal == HierarchyReveal::center) offset = begin + (request.row_height - request.height) / 2;
            else if (begin < offset) offset = begin;
            else if (end > offset + request.height) offset = end - request.height;
            result.revealed_key = request.reveal_key;
        }
        offset = std::clamp(offset, 0., std::max(0., result.content_height - request.height)); result.offset = offset;
        if (!count || request.width == 0 || request.height == 0) return result;
        const auto visible_first = std::min(count - 1, std::size_t(std::floor(offset / request.row_height)));
        const auto visible_end = std::min(count, std::size_t(std::ceil((offset + request.height) / request.row_height)));
        const auto first = visible_first > request.overscan ? visible_first - request.overscan : 0;
        const auto last = std::min(count, visible_end + request.overscan);
        result.rows.reserve(last - first);
        for (auto index = first; index < last; ++index) result.rows.push_back({index, index * request.row_height - offset, request.row_height});
        result.anchor = {projection_->entries[visible_first].entry.key, visible_first,
                         offset - visible_first * request.row_height, request.movement};
        return result;
    }
private:
    explicit HierarchyViewportSource(std::shared_ptr<const HierarchyProjection> projection) : projection_(std::move(projection)) {}
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    std::shared_ptr<const HierarchyProjection> projection_;
    std::unordered_map<std::string_view, std::size_t> positions_;
};

} // namespace lvui
