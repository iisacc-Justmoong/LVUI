#pragma once

#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyViewport.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct HierarchyDragPoint final { double x = 0, y = 0; };
struct HierarchyDragViewport final {
    std::shared_ptr<const HierarchyViewportSource> source;
    double width = 0, height = 0, offset = 0, row_height = 20;
};
struct HierarchyDragProposal final {
    HierarchyMove request;
    HierarchyMovePlan plan;
};
struct HierarchyDragUpdate final {
    bool dragging = false;
    int scroll_direction = 0;
    std::optional<HierarchyDragProposal> proposal;
};

// A drag previews an immutable snapshot; only an injected HierarchyEditor can
// commit the eventual revision-checked request. Pointer motion is view state.
class HierarchyDrag final {
public:
    [[nodiscard]] static HierarchyResult<HierarchyDrag> begin(std::shared_ptr<const HierarchySnapshot> snapshot,
            std::string key, HierarchyDragPoint origin, double threshold = 4, double scroll_edge = 16) {
        if (!point(origin) || !std::isfinite(threshold) || threshold < 0 || threshold > 256 ||
            !std::isfinite(scroll_edge) || scroll_edge < 0 || scroll_edge > 8192)
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy drag configuration");
        auto index = HierarchyIndex::create(std::move(snapshot)); if (!index) return std::unexpected(index.error());
        const auto found = index->find(key);
        if (!found) return error(HierarchyErrorCode::not_found, "Hierarchy drag source was not found");
        const auto& entry = index->snapshot().entries[*found];
        if (!entry.enabled || !entry.draggable) return error(HierarchyErrorCode::invalid, "Hierarchy drag source cannot move");
        return HierarchyDrag(std::move(*index), std::move(key), origin, threshold, scroll_edge);
    }
    [[nodiscard]] HierarchyResult<HierarchyDragUpdate> update(HierarchyDragPoint position, const HierarchyDragViewport& viewport) {
        if (!active_) return error(HierarchyErrorCode::invalid, "Hierarchy drag has ended");
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        if (!point(position) || !viewport.source || !bound(viewport.width, 0, 8192) || !bound(viewport.height, 0, 8192) ||
            !bound(viewport.offset, 0, 1e15) || !bound(viewport.row_height, 1, 8192)) {
            proposal_.reset(); return error(HierarchyErrorCode::invalid, "Invalid hierarchy drag viewport");
        }
        const auto& projection = viewport.source->projection();
        if (projection.source_revision != index_.snapshot().revision) {
            proposal_.reset(); active_ = false; return error(HierarchyErrorCode::stale, "Hierarchy changed during the drag");
        }
        if (!dragging_ && std::hypot(position.x - origin_.x, position.y - origin_.y) >= threshold_) dragging_ = true;
        proposal_.reset(); HierarchyDragUpdate result; result.dragging = dragging_;
        if (!dragging_ || viewport.width == 0 || viewport.height == 0) return result;
        const auto edge = std::min(scroll_edge_, viewport.height / 2);
        if (position.y < edge && viewport.offset > 0) result.scroll_direction = -1;
        else if (position.y > viewport.height - edge && viewport.offset + viewport.height < projection.entries.size() * viewport.row_height)
            result.scroll_direction = 1;
        if (position.x < 0 || position.x >= viewport.width || position.y < 0 || position.y >= viewport.height) return result;
        const auto logical = viewport.offset + position.y;
        HierarchyMove request{.key=key_, .revision=index_.snapshot().revision};
        if (logical >= projection.entries.size() * viewport.row_height) request.placement = HierarchyPlacement::root_end;
        else {
            const auto row = std::size_t(std::floor(logical / viewport.row_height));
            const auto& target = projection.entries[row].entry; if (!target.enabled) return result;
            const auto fraction = (logical - row * viewport.row_height) / viewport.row_height;
            request.target = target.key;
            request.placement = fraction < .25 ? HierarchyPlacement::before :
                                fraction >= .75 ? HierarchyPlacement::after : HierarchyPlacement::child;
        }
        auto plan = index_.plan(request);
        if (!plan || plan->unchanged) return result;
        proposal_ = HierarchyDragProposal{std::move(request), std::move(*plan)};
        result.proposal = proposal_; return result;
    }
    [[nodiscard]] HierarchyResult<std::optional<HierarchyDragProposal>> finish() {
        if (!active_) return error(HierarchyErrorCode::invalid, "Hierarchy drag has ended");
        active_ = false; return std::exchange(proposal_, std::nullopt);
    }
    void cancel() noexcept { active_ = false; proposal_.reset(); }
private:
    HierarchyDrag(HierarchyIndex index, std::string key, HierarchyDragPoint origin, double threshold, double edge)
        : index_(std::move(index)), key_(std::move(key)), origin_(origin), threshold_(threshold), scroll_edge_(edge) {}
    [[nodiscard]] static bool point(HierarchyDragPoint p) noexcept {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e9 && std::abs(p.y) <= 1e9;
    }
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    HierarchyIndex index_;
    std::string key_;
    HierarchyDragPoint origin_;
    double threshold_ = 4, scroll_edge_ = 16;
    std::optional<HierarchyDragProposal> proposal_;
    bool active_ = true, dragging_ = false;
};

} // namespace lvui
