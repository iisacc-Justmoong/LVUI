#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace lvui::native {

// Application-level commands own no native window. The caller supplies a
// deferred dispatcher and a replaceable close operation on the owner thread.
class ApplicationCommands final : public std::enable_shared_from_this<ApplicationCommands> {
public:
    using Task = std::function<void()>;
    using Dispatch = std::function<ii::core::Result<void>(Task)>;
    using Close = std::function<ii::core::Result<void>()>;
    using ErrorHandler = std::function<void(const ii::core::Error&)>;

    ApplicationCommands(Dispatch dispatch, Close close, ErrorHandler error)
        : dispatch_(std::move(dispatch)), close_(std::move(close)), on_error_(std::move(error)) {
        if (!dispatch_ || !close_ || !on_error_) throw std::invalid_argument("Application commands require dispatch, close and failure handlers");
    }
    ApplicationCommands(const ApplicationCommands&) = delete;
    ApplicationCommands& operator=(const ApplicationCommands&) = delete;

    [[nodiscard]] ii::core::Result<void> quit() {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        if (!accepting_) return ii::core::fail(ii::core::ErrorCode::closed, "Application commands are closed");
        if (pending_ || delivering_) return {};
        auto weak = weak_from_this();
        if (weak.expired()) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application commands require shared ownership");
        std::shared_ptr<Admission> admission;
        try {
            admission = std::make_shared<Admission>();
            auto submitted = dispatch_([weak, admission] {
                if (admission->submitting.load()) { admission->inline_delivery = true; return; }
                if (!admission->accepted.load()) return;
                if (auto commands = weak.lock()) commands->deliver();
            });
            if (!submitted) { admission->submitting = false; return submitted; }
            if (admission->inline_delivery) {
                admission->submitting = false;
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application command dispatcher must defer delivery");
            }
            pending_ = true;
            admission->accepted = true;
            admission->submitting = false;
            if (admission->inline_delivery) {
                admission->accepted = false; pending_ = false;
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application command dispatcher must defer delivery");
            }
            return {};
        } catch (...) {
            if (admission) { admission->accepted = false; admission->submitting = false; }
            pending_ = false;
            return ii::core::fail(ii::core::ErrorCode::failed, "Application command dispatch failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::core::Result<void> cancel() {
        if (auto owner = on_owner(); !owner) return owner;
        accepting_ = false; pending_ = false;
        if (!delivering_) { close_ = {}; on_error_ = {}; }
        return {};
    }
    [[nodiscard]] ii::core::Result<void> outcome() const {
        if (auto owner = on_owner(); !owner) return owner;
        if (failure_) return std::unexpected(*failure_);
        return {};
    }

private:
    struct Admission final { std::atomic_bool submitting{true}, accepted{false}, inline_delivery{false}; };
    [[nodiscard]] ii::core::Result<void> on_owner() const {
        if (owner_ != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application commands require their owner thread");
        return {};
    }
    void deliver() noexcept {
        if (owner_ != std::this_thread::get_id() || !accepting_ || !pending_ || delivering_) return;
        pending_ = false; delivering_ = true;
        std::optional<ii::core::Error> error;
        try {
            auto close = close_;
            if (auto result = close(); !result) error = std::move(result.error());
        } catch (...) {
            error = ii::core::Error{ii::core::ErrorCode::failed, "Application close command failed", std::current_exception()};
        }
        if (error) {
            if (!failure_) failure_ = std::move(*error);
            accepting_ = false;
            try { auto report = on_error_; if (report) report(*failure_); } catch (...) {}
        }
        delivering_ = false;
        if (!accepting_) { close_ = {}; on_error_ = {}; }
    }

    const std::thread::id owner_ = std::this_thread::get_id();
    Dispatch dispatch_;
    Close close_;
    ErrorHandler on_error_;
    std::optional<ii::core::Error> failure_;
    bool accepting_ = true, pending_ = false, delivering_ = false;
};

} // namespace lvui::native
