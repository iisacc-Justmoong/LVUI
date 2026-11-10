#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define LVUI_APPLICATION_FILE_EVENTS_API 1

namespace lvui::native {

// Owner-thread queue for native file-open batches. The application decides
// when it is ready to drain; no operating-system or GUI dependency is owned.
class ApplicationFileEvents final {
public:
    using Files = std::vector<std::string>;
    using Delivery = std::function<ii::core::Result<void>(const Files&)>;
    static constexpr std::size_t maximum_batches = 64, maximum_files_per_batch = 256;
    static constexpr std::size_t maximum_path_bytes = 131072, maximum_queued_bytes = 4 * 1024 * 1024;

    explicit ApplicationFileEvents(Delivery delivery) : delivery_(std::move(delivery)) {
        if (!delivery_) throw std::invalid_argument("File events require a delivery callback");
    }
    ApplicationFileEvents(const ApplicationFileEvents&) = delete;
    ApplicationFileEvents& operator=(const ApplicationFileEvents&) = delete;

    [[nodiscard]] ii::core::Result<void> submit(Files files) {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        if (closed_) return ii::core::fail(ii::core::ErrorCode::closed, "File event delivery is closed");
        if (files.empty()) return {};
        if (files.size() > maximum_files_per_batch)
            return abort({ii::core::ErrorCode::invalid_argument, "Too many paths in a file event"});
        std::size_t bytes = 0;
        for (const auto& path : files) {
            if (path.empty() || path.size() > maximum_path_bytes || path.find('\0') != path.npos || !ii::core::valid_utf8(path))
                return abort({ii::core::ErrorCode::invalid_argument, "File event requires nonempty UTF-8 paths"});
            if (path.size() > maximum_queued_bytes - bytes)
                return abort({ii::core::ErrorCode::full, "File event byte limit exceeded"});
            bytes += path.size();
        }
        if (queue_.size() == maximum_batches || bytes > maximum_queued_bytes - queued_bytes_)
            return abort({ii::core::ErrorCode::full, "File event queue is full"});
        try {
            queue_.push_back({std::move(files), bytes});
            queued_bytes_ += bytes;
            return {};
        } catch (...) {
            return abort({ii::core::ErrorCode::failed, "Cannot enqueue file event", std::current_exception()});
        }
    }

    [[nodiscard]] ii::core::Result<void> drain() {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        if (closed_) return ii::core::fail(ii::core::ErrorCode::closed, "File event delivery is closed");
        if (delivering_) return ii::core::fail(ii::core::ErrorCode::busy, "File delivery is already active");
        delivering_ = true; Reset reset{delivering_};
        for (std::size_t count = 0; count < maximum_batches && !queue_.empty(); ++count) {
            auto next = std::move(queue_.front()); queue_.pop_front(); queued_bytes_ -= next.bytes;
            try {
                if (auto delivered = delivery_(next.files); !delivered) return abort(std::move(delivered.error()));
            } catch (...) {
                return abort({ii::core::ErrorCode::failed, "Application file callback failed", std::current_exception()});
            }
            if (failure_) return std::unexpected(*failure_);
        }
        if (!queue_.empty()) return abort({ii::core::ErrorCode::full, "File event reentry limit exceeded"});
        return {};
    }

    [[nodiscard]] ii::core::Result<void> abort(ii::core::Error error) {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        if (closed_ && !delivering_) return ii::core::fail(ii::core::ErrorCode::closed, "File event delivery is closed");
        failure_ = std::move(error);
        queue_.clear(); queued_bytes_ = 0;
        return std::unexpected(*failure_);
    }
    [[nodiscard]] ii::core::Result<void> outcome() const {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        return {};
    }
    // Closing admission is safe inside a delivery; the active callable is
    // released only by close() after it has returned.
    [[nodiscard]] ii::core::Result<void> cancel() {
        if (auto owner = on_owner(); !owner) return owner;
        closed_ = true; queue_.clear(); queued_bytes_ = 0;
        return {};
    }
    [[nodiscard]] ii::core::Result<void> close() {
        if (auto owner = on_owner(); !owner) return owner;
        if (delivering_) return ii::core::fail(ii::core::ErrorCode::busy, "File delivery is active");
        if (auto cancelled = cancel(); !cancelled) return cancelled;
        delivery_ = {};
        return {};
    }
    [[nodiscard]] bool pending() const noexcept { return !queue_.empty(); }

private:
    struct Batch final { Files files; std::size_t bytes; };
    struct Reset final { bool& active; ~Reset() { active = false; } };
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (thread_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "File events require their owner thread");
        return {};
    }
    const std::thread::id thread_ = std::this_thread::get_id();
    Delivery delivery_;
    std::deque<Batch> queue_;
    std::size_t queued_bytes_ = 0;
    std::optional<ii::core::Error> failure_;
    bool closed_ = false, delivering_ = false;
};

} // namespace lvui::native
