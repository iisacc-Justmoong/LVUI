#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <atomic>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

// The owner retains one native window generation. No UI framework is required
// by the registry; the application supplies the host adapter and dispatcher.
struct SceneInstance final {
    std::shared_ptr<void> owner;
    std::function<ii::core::Result<bool>()> is_closed;
    std::function<ii::core::Result<void>()> request_close;
};

class SceneRegistry final : public std::enable_shared_from_this<SceneRegistry> {
public:
    using Task = std::function<void()>;
    using Dispatch = std::function<ii::core::Result<void>(Task)>;
    using ErrorHandler = std::function<void(const ii::core::Error&)>;
    using Factory = std::function<ii::core::Result<SceneInstance>(bool initial)>;
    static constexpr std::size_t maximum_scenes = 4096, maximum_id_bytes = 1024;

    SceneRegistry(Dispatch dispatch, ErrorHandler on_error)
        : dispatch_(std::move(dispatch)), on_error_(std::move(on_error)) {
        if (!dispatch_ || !on_error_) throw std::invalid_argument("Scene registry requires a dispatcher and error handler");
    }
    SceneRegistry(const SceneRegistry&) = delete;
    SceneRegistry& operator=(const SceneRegistry&) = delete;

    [[nodiscard]] ii::core::Result<void> define(std::string id, Factory factory) {
        if (auto owner = on_owner(); !owner) return owner;
        if (!accepting_) return ii::core::fail(ii::core::ErrorCode::closed, "Scene registry is closed");
        if (sealed_) return ii::core::fail(ii::core::ErrorCode::busy, "Scene declarations are sealed");
        if (id.empty() || id.size() > maximum_id_bytes || id.find('\0') != id.npos || !ii::core::valid_utf8(id) || !factory)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene requires a valid UTF-8 id and factory");
        if (index_.contains(id)) return ii::core::fail(ii::core::ErrorCode::already_exists, "Duplicate scene id");
        if (entries_.size() == maximum_scenes) return ii::core::fail(ii::core::ErrorCode::full, "Scene registry is full");
        try {
            const auto index = entries_.size();
            entries_.push_back({std::move(id), std::move(factory), {}});
            try { index_.emplace(entries_.back().id, index); }
            catch (...) { entries_.pop_back(); throw; }
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Cannot register scene", std::current_exception()); }
        return {};
    }

    // Startup opens are synchronous; later requests are always dispatched.
    [[nodiscard]] ii::core::Result<void> open_initial(std::string_view id) {
        if (auto owner = on_owner(); !owner) return owner;
        if (!accepting_) return ii::core::fail(ii::core::ErrorCode::closed, "Scene registry is closed");
        auto found = index_.find(id);
        if (found == index_.end()) return ii::core::fail(ii::core::ErrorCode::not_found, "Unknown scene id");
        sealed_ = true;
        return open_now(found->second, true);
    }
    [[nodiscard]] ii::core::Result<void> open(std::string_view id) { return request(id, true); }
    [[nodiscard]] ii::core::Result<void> close(std::string_view id) { return request(id, false); }

    // Stop admitting or delivering requests without closing a host from inside
    // its native close callback. shutdown() subsequently retires all scenes.
    [[nodiscard]] ii::core::Result<void> cancel() {
        if (auto owner = on_owner(); !owner) return owner;
        accepting_ = false;
        return {};
    }

    // Refuses further work before asking live generations to close. A vetoed or
    // failed close stays retained and a subsequent shutdown can retry it.
    [[nodiscard]] ii::core::Result<void> shutdown() {
        if (auto owner = on_owner(); !owner) return owner;
        if (busy_) return ii::core::fail(ii::core::ErrorCode::busy, "Scene operation is in progress");
        accepting_ = false;
        busy_ = true; Reset reset{busy_};
        std::optional<ii::core::Error> first;
        for (auto& entry : entries_) {
            if (!entry.instance) continue;
            auto result = close_instance(entry);
            if (!result && !first) first = result.error();
            if (entry.instance) {
                auto closed = closed_state(*entry.instance);
                if (!closed) { if (!first) first = closed.error(); }
                else if (*closed) entry.instance.reset();
                else if (!first) first = ii::core::Error{ii::core::ErrorCode::busy, "Scene remained open during shutdown"};
            }
        }
        if (first) return std::unexpected(std::move(*first));
        return {};
    }

private:
    struct Entry final { std::string id; Factory factory; std::optional<SceneInstance> instance; };
    struct Reset final { bool& value; ~Reset() { value = false; } };
    struct Admission final { std::atomic_bool submitting{true}, accepted{false}, inline_delivery{false}; };

    [[nodiscard]] ii::core::Result<void> request(std::string_view id, bool opening) {
        if (auto owner = on_owner(); !owner) return owner;
        if (!accepting_) return ii::core::fail(ii::core::ErrorCode::closed, "Scene registry is closed");
        auto found = index_.find(id);
        if (found == index_.end()) return ii::core::fail(ii::core::ErrorCode::not_found, "Unknown scene id");
        auto weak = weak_from_this();
        if (weak.expired()) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene registry requires shared ownership");
        sealed_ = true;
        try {
            auto admission = std::make_shared<Admission>();
            auto submitted = dispatch_([weak, admission, index = found->second, opening] {
                if (admission->submitting.load()) { admission->inline_delivery = true; return; }
                if (!admission->accepted.load()) return;
                if (auto self = weak.lock()) {
                    if (auto owner = self->on_owner(); !owner) return;
                    if (!self->accepting_) return;
                    auto result = opening ? self->open_now(index, false) : self->close_now(index);
                    if (!result) self->report(result.error());
                }
            });
            if (!submitted) { admission->submitting = false; return submitted; }
            if (admission->inline_delivery) {
                admission->submitting = false;
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene dispatcher must defer delivery");
            }
            admission->accepted = true;
            admission->submitting = false;
            if (admission->inline_delivery) {
                admission->accepted = false;
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene dispatcher must defer delivery");
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Scene request dispatch failed", std::current_exception());
        }
    }
    [[nodiscard]] ii::core::Result<void> open_now(std::size_t index, bool initial) {
        if (busy_) return ii::core::fail(ii::core::ErrorCode::busy, "Scene operation is in progress");
        busy_ = true; Reset reset{busy_};
        auto& entry = entries_[index];
        try {
            if (entry.instance) {
                auto closed = closed_state(*entry.instance);
                if (!closed) return std::unexpected(closed.error());
                if (!*closed) return {};
                entry.instance.reset();
            }
            auto created = entry.factory(initial);
            if (!created) return std::unexpected(created.error());
            if (!created->owner || !created->is_closed || !created->request_close)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Scene factory returned an incomplete instance");
            entry.instance = std::move(*created);
            return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Scene creation failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> close_now(std::size_t index) {
        if (busy_) return ii::core::fail(ii::core::ErrorCode::busy, "Scene operation is in progress");
        busy_ = true; Reset reset{busy_};
        return close_instance(entries_[index]);
    }
    [[nodiscard]] static ii::core::Result<bool> closed_state(const SceneInstance& instance) {
        try { return instance.is_closed(); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Scene state query failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<void> close_instance(Entry& entry) {
        if (!entry.instance) return {};
        auto closed = closed_state(*entry.instance);
        if (!closed) return std::unexpected(closed.error());
        if (*closed) return {};
        try { return entry.instance->request_close(); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Scene close request failed", std::current_exception()); }
    }
    void report(const ii::core::Error& error) noexcept {
        if (!accepting_) return;
        accepting_ = false;
        try { auto handler = on_error_; if (handler) handler(error); } catch (...) {}
    }
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Scene registry requires its owner thread");
        return {};
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    Dispatch dispatch_;
    ErrorHandler on_error_;
    std::vector<Entry> entries_;
    std::map<std::string, std::size_t, std::less<>> index_;
    bool accepting_ = true, sealed_ = false, busy_ = false;
};

} // namespace lvui::native
