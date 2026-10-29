#pragma once

#include "Text.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lvui {

inline constexpr std::size_t hierarchy_maximum_items = 65536;
enum class HierarchyErrorCode { invalid, not_found, stale, exhausted, wrong_thread };
struct HierarchyError final { HierarchyErrorCode code; std::string message; };
template<class T> using HierarchyResult = std::expected<T, HierarchyError>;

struct HierarchyEntry final {
    std::string key, parent, label, icon_name;
    std::optional<std::int64_t> id;
    std::int64_t count = -1;
    bool enabled = true, selectable = true, draggable = true;
    friend bool operator==(const HierarchyEntry&, const HierarchyEntry&) = default;
};
struct HierarchySnapshot final {
    std::vector<HierarchyEntry> entries;
    std::uint64_t revision = 0;
};

// Read and edit ports are independent so applications can project their own
// models, keep them immutable, or route structural changes to another module.
class HierarchySource {
public:
    virtual ~HierarchySource() = default;
    [[nodiscard]] virtual HierarchyResult<std::shared_ptr<const HierarchySnapshot>> snapshot() const = 0;
};
enum class HierarchyPlacement { before, after, child, root_end };
struct HierarchyMove final {
    std::string key, target;
    HierarchyPlacement placement = HierarchyPlacement::before;
    std::uint64_t revision = 0;
};
class HierarchyEditor {
public:
    virtual ~HierarchyEditor() = default;
    [[nodiscard]] virtual HierarchyResult<std::uint64_t> move(const HierarchyMove&) = 0;
};

struct HierarchyMovePlan final {
    std::string key, old_parent, new_parent, before_sibling;
    std::size_t first = 0, last = 0, insertion = 0;
    std::uint64_t revision = 0;
    bool unchanged = false;
};

// Validated preorder index. Each subtree is one half-open range, so a move
// can preserve all descendants without inventing relationships or cycles.
class HierarchyIndex final {
public:
    struct Item final { std::size_t depth = 0, end = 0, children = 0; };
    [[nodiscard]] static HierarchyResult<HierarchyIndex> create(std::shared_ptr<const HierarchySnapshot> snapshot) {
        if (!snapshot || snapshot->entries.size() > hierarchy_maximum_items)
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy snapshot size");
        HierarchyIndex index; index.snapshot_ = std::move(snapshot);
        index.items_.resize(index.snapshot_->entries.size());
        index.positions_.reserve(index.items_.size());
        std::unordered_map<std::string_view, std::size_t> active; active.reserve(index.items_.size());
        std::vector<std::size_t> stack; stack.reserve(index.items_.size()); std::size_t bytes = 0;
        const auto pop = [&](std::size_t end) {
            const auto previous = stack.back(); stack.pop_back();
            index.items_[previous].end = end; active.erase(index.snapshot_->entries[previous].key);
        };
        for (std::size_t i = 0; i < index.snapshot_->entries.size(); ++i) {
            const auto& entry = index.snapshot_->entries[i];
            if (entry.key.empty() || entry.key.size() > 1024 || !valid_accessibility_text(entry.key) ||
                !valid_accessibility_text(entry.parent) || !valid_accessibility_text(entry.label) ||
                !valid_accessibility_text(entry.icon_name) || entry.count < -1)
                return error(HierarchyErrorCode::invalid, "Invalid hierarchy entry");
            const auto length = entry.key.size() + entry.parent.size() + entry.label.size() + entry.icon_name.size();
            if (length > 64 * 1024 * 1024 - bytes) return error(HierarchyErrorCode::invalid, "Hierarchy text exceeds its memory limit");
            bytes += length;
            if (!index.positions_.emplace(entry.key, i).second) return error(HierarchyErrorCode::invalid, "Duplicate hierarchy key");
            if (entry.parent.empty()) {
                while (!stack.empty()) pop(i);
            } else {
                const auto parent = active.find(entry.parent);
                if (parent == active.end()) return error(HierarchyErrorCode::invalid, "Hierarchy parent must be an active preorder ancestor");
                while (stack.size() > parent->second + 1) pop(i);
                ++index.items_[stack.back()].children;
            }
            index.items_[i].depth = stack.size(); active.emplace(entry.key, stack.size()); stack.push_back(i);
        }
        while (!stack.empty()) pop(index.items_.size());
        return index;
    }
    [[nodiscard]] const HierarchySnapshot& snapshot() const noexcept { return *snapshot_; }
    [[nodiscard]] const std::vector<Item>& items() const noexcept { return items_; }
    [[nodiscard]] std::optional<std::size_t> find(std::string_view key) const {
        const auto found = positions_.find(key); return found == positions_.end() ? std::nullopt : std::optional(found->second);
    }
    [[nodiscard]] HierarchyResult<HierarchyMovePlan> plan(const HierarchyMove& request) const {
        if (request.revision != snapshot_->revision) return error(HierarchyErrorCode::stale, "Hierarchy changed before the move");
        const auto source = find(request.key); if (!source) return error(HierarchyErrorCode::not_found, "Hierarchy source was not found");
        if (!snapshot_->entries[*source].draggable) return error(HierarchyErrorCode::invalid, "Hierarchy source cannot be moved");
        std::size_t destination = snapshot_->entries.size(); std::string parent, before;
        if (request.placement != HierarchyPlacement::root_end) {
            if (request.placement != HierarchyPlacement::before && request.placement != HierarchyPlacement::after && request.placement != HierarchyPlacement::child)
                return error(HierarchyErrorCode::invalid, "Invalid hierarchy move placement");
            const auto target = find(request.target); if (!target) return error(HierarchyErrorCode::not_found, "Hierarchy target was not found");
            if (*target >= *source && *target < items_[*source].end)
                return error(HierarchyErrorCode::invalid, "A hierarchy subtree cannot move into itself");
            const auto& entry = snapshot_->entries[*target];
            if (request.placement == HierarchyPlacement::before) { destination = *target; parent = entry.parent; before = entry.key; }
            else if (request.placement == HierarchyPlacement::after) {
                destination = items_[*target].end; parent = entry.parent;
                if (destination < items_.size() && snapshot_->entries[destination].parent == parent) before = snapshot_->entries[destination].key;
            } else { destination = items_[*target].end; parent = entry.key; }
        } else if (!request.target.empty()) return error(HierarchyErrorCode::invalid, "Root-end placement does not take a target");
        const auto last = items_[*source].end, length = last - *source;
        const auto insertion = destination > *source ? destination - length : destination;
        const bool unchanged = insertion == *source && snapshot_->entries[*source].parent == parent;
        return HierarchyMovePlan{request.key, snapshot_->entries[*source].parent, std::move(parent), std::move(before),
                                 *source, last, insertion, request.revision, unchanged};
    }
private:
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    std::shared_ptr<const HierarchySnapshot> snapshot_;
    std::vector<Item> items_;
    std::unordered_map<std::string_view, std::size_t> positions_;
};

// Optional in-memory implementation. Snapshots remain immutable after a
// replacement; commands compare the source revision under the same lock.
class HierarchyMemory final : public HierarchySource, public HierarchyEditor {
public:
    explicit HierarchyMemory(std::vector<HierarchyEntry> entries = {}) {
        auto initial = std::make_shared<HierarchySnapshot>(HierarchySnapshot{std::move(entries), 0});
        auto valid = HierarchyIndex::create(initial); if (!valid) throw std::invalid_argument(valid.error().message);
        snapshot_ = std::move(initial);
    }
    [[nodiscard]] HierarchyResult<std::shared_ptr<const HierarchySnapshot>> snapshot() const override {
        std::scoped_lock lock(mutex_); return snapshot_;
    }
    [[nodiscard]] HierarchyResult<std::uint64_t> replace(std::vector<HierarchyEntry> entries, std::uint64_t expected) {
        auto candidate = std::make_shared<HierarchySnapshot>(HierarchySnapshot{std::move(entries), 0});
        if (auto valid = HierarchyIndex::create(candidate); !valid) return std::unexpected(valid.error());
        std::scoped_lock lock(mutex_);
        if (snapshot_->revision != expected) return error(HierarchyErrorCode::stale, "Hierarchy changed before replacement");
        if (snapshot_->entries == candidate->entries) return snapshot_->revision;
        if (snapshot_->revision == std::numeric_limits<std::uint64_t>::max()) return error(HierarchyErrorCode::exhausted, "Hierarchy revision exhausted");
        candidate->revision = snapshot_->revision + 1; snapshot_ = std::move(candidate); return snapshot_->revision;
    }
    [[nodiscard]] HierarchyResult<std::uint64_t> move(const HierarchyMove& request) override {
        std::scoped_lock lock(mutex_);
        auto index = HierarchyIndex::create(snapshot_); if (!index) return std::unexpected(index.error());
        auto plan = index->plan(request); if (!plan) return std::unexpected(plan.error());
        if (plan->unchanged) return snapshot_->revision;
        if (snapshot_->revision == std::numeric_limits<std::uint64_t>::max()) return error(HierarchyErrorCode::exhausted, "Hierarchy revision exhausted");
        auto entries = snapshot_->entries;
        std::vector<HierarchyEntry> moving(entries.begin() + plan->first, entries.begin() + plan->last);
        moving.front().parent = plan->new_parent;
        entries.erase(entries.begin() + plan->first, entries.begin() + plan->last);
        entries.insert(entries.begin() + plan->insertion, std::make_move_iterator(moving.begin()), std::make_move_iterator(moving.end()));
        auto candidate = std::make_shared<HierarchySnapshot>(HierarchySnapshot{std::move(entries), snapshot_->revision + 1});
        if (auto valid = HierarchyIndex::create(candidate); !valid) return std::unexpected(valid.error());
        snapshot_ = std::move(candidate); return snapshot_->revision;
    }
private:
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    mutable std::mutex mutex_;
    std::shared_ptr<const HierarchySnapshot> snapshot_;
};

struct HierarchyVisibleEntry final {
    HierarchyEntry entry;
    std::size_t source_index = 0, depth = 0, child_count = 0;
    bool expanded = false, selected = false;
};
struct HierarchyProjection final {
    std::vector<HierarchyVisibleEntry> entries;
    std::uint64_t source_revision = 0, view_revision = 0;
};

// Expansion and selection are view state only. They never call the source's
// edit port and never publish a document revision.
class HierarchyController final {
public:
    explicit HierarchyController(std::shared_ptr<const HierarchySource> source) : source_(std::move(source)) {
        if (!source_) throw std::invalid_argument("Hierarchy source is required");
    }
    [[nodiscard]] HierarchyResult<bool> set_expanded(std::string_view key, bool expanded) {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        const auto found = index_->find(key); if (!found) return error(HierarchyErrorCode::not_found, "Hierarchy entry was not found");
        if (!index_->items()[*found].children) return error(HierarchyErrorCode::invalid, "A leaf cannot be expanded");
        const bool current = expanded_.contains(std::string(key)); if (current == expanded) return false;
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) return error(HierarchyErrorCode::exhausted, "Hierarchy view revision exhausted");
        if (expanded) expanded_.emplace(key); else expanded_.erase(std::string(key));
        ++revision_; return true;
    }
    [[nodiscard]] HierarchyResult<bool> select(std::string_view key) {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        if (!key.empty()) {
            const auto found = index_->find(key); if (!found) return error(HierarchyErrorCode::not_found, "Hierarchy entry was not found");
            const auto& entry = index_->snapshot().entries[*found];
            if (!entry.enabled || !entry.selectable) return error(HierarchyErrorCode::invalid, "Hierarchy entry cannot be selected");
        }
        if (selected_ == key) return false;
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) return error(HierarchyErrorCode::exhausted, "Hierarchy view revision exhausted");
        selected_ = key; ++revision_; return true;
    }
    [[nodiscard]] HierarchyResult<HierarchyProjection> projection() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        HierarchyProjection result; result.source_revision = index_->snapshot().revision; result.view_revision = revision_;
        const auto& entries = index_->snapshot().entries; const auto& items = index_->items();
        for (std::size_t i = 0; i < entries.size();) {
            const auto& entry = entries[i]; const bool expanded = expanded_.contains(entry.key);
            result.entries.push_back({entry, i, items[i].depth, items[i].children, expanded, selected_ == entry.key});
            i = expanded ? i + 1 : items[i].end;
        }
        return result;
    }
    [[nodiscard]] HierarchyResult<std::shared_ptr<const HierarchySnapshot>> source_snapshot() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        return index_snapshot_;
    }
    [[nodiscard]] HierarchyResult<std::uint64_t> source_revision() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        return index_->snapshot().revision;
    }
    [[nodiscard]] HierarchyResult<std::string> selected_key() const {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyErrorCode::wrong_thread, "Hierarchy controller requires its owner thread");
        return selected_;
    }
private:
    [[nodiscard]] HierarchyResult<void> refresh() {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyErrorCode::wrong_thread, "Hierarchy controller requires its owner thread");
        auto snapshot = source_->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        if (index_ && index_snapshot_ == *snapshot) return {};
        auto next = HierarchyIndex::create(*snapshot); if (!next) return std::unexpected(next.error());
        bool pruned = !selected_.empty() && !next->find(selected_);
        for (const auto& key : expanded_) if (!next->find(key)) { pruned = true; break; }
        if (pruned && revision_ == std::numeric_limits<std::uint64_t>::max()) return error(HierarchyErrorCode::exhausted, "Hierarchy view revision exhausted");
        if (pruned) {
            if (!selected_.empty() && !next->find(selected_)) selected_.clear();
            std::erase_if(expanded_, [&](const auto& key) { return !next->find(key); }); ++revision_;
        }
        index_ = std::move(*next); index_snapshot_ = std::move(*snapshot); return {};
    }
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    std::shared_ptr<const HierarchySource> source_;
    std::shared_ptr<const HierarchySnapshot> index_snapshot_;
    std::optional<HierarchyIndex> index_;
    std::unordered_set<std::string> expanded_;
    std::string selected_;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::uint64_t revision_ = 0;
};

} // namespace lvui
