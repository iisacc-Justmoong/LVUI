#pragma once

#include <LVUI/Hierarchy.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace lvui::native {
struct HierarchyRowSnapshot final {
    HierarchyEntry entry;
    std::size_t source_index = 0, depth = 0, child_count = 0;
    bool expanded = false, selected = false, enabled = true;
    double height = 20;
    friend bool operator==(const HierarchyRowSnapshot&, const HierarchyRowSnapshot&) = default;
};

// A read-only observable belonging to one mounted row generation. The list
// publishes updates after mounting and closes it when that row retires.
class HierarchyRowContext final {
public:
    [[nodiscard]] const HierarchyRowSnapshot& get() const { check(); return value_->get(); }
    [[nodiscard]] std::uint64_t revision() const { check(); return value_->revision(); }
    [[nodiscard]] ii::core::ObservableSubscription observe(
            std::function<void(const HierarchyRowSnapshot&, std::uint64_t)> callback, bool initial = false) const {
        check(); return value_->observe(std::move(callback), initial);
    }
    [[nodiscard]] ii::core::ObservableRead read_dependency() const { check(); return value_->read_dependency(); }
private:
    friend struct HierarchyRowContextPort;
    explicit HierarchyRowContext(HierarchyRowSnapshot initial) : value_(std::in_place, std::move(initial)) {}
    void check() const {
        if (owner_ != std::this_thread::get_id()) throw std::logic_error("Hierarchy row context requires its owner thread");
        if (!value_) throw std::logic_error("Hierarchy row context is closed");
    }
    void publish(HierarchyRowSnapshot next) { check(); value_->set(std::move(next)); }
    void close() noexcept { value_.reset(); }
    const std::thread::id owner_ = std::this_thread::get_id();
    std::optional<ii::core::Observable<HierarchyRowSnapshot>> value_;
};
struct HierarchyRowContextPort final {
    [[nodiscard]] static std::shared_ptr<HierarchyRowContext> make(HierarchyRowSnapshot value){return std::shared_ptr<HierarchyRowContext>(new HierarchyRowContext(std::move(value)));}
    static void publish(HierarchyRowContext& context,HierarchyRowSnapshot value){context.publish(std::move(value));}
    static void close(HierarchyRowContext& context)noexcept{context.close();}
};
} // namespace lvui::native
