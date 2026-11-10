#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Runtime/Query.h>
#include <cstdint>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct SessionRead final {};
struct SessionCommand final {};
struct SessionRecord final {
    static inline int destroyed_during_query = 0;
    ii::core::Observable<std::string> title{"Received object"};
    ii::core::Observable<std::int64_t> number{10};
    bool querying = false;
    std::function<void()> onRead;
    ii::runtime::QueryPort<SessionRead, std::int64_t> readPort{[this](const SessionRead&) {
        struct Guard { bool& active; explicit Guard(bool& value) : active(value) { active = true; } ~Guard() { active = false; } } guard(querying);
        if (auto callback = std::exchange(onRead, {})) callback();
        return number.get();
    }};
    ii::runtime::QueryPort<SessionCommand> commandPort{[this](const SessionCommand&) { number.set(number.get() + 1); }};
    ~SessionRecord() { if (querying) ++destroyed_during_query; }
    SessionRead readRequest(std::int64_t) const { return {}; }
    SessionCommand commandRequest() const { return {}; }
};
struct SessionQueryBackend final {
    ii::core::Observable<std::int64_t> number{4}, trigger{0};
    ii::core::Observable<std::int64_t> sessionCount{0}, numericValue{0};
    ii::core::Observable<std::string> sessionTitle{"Session object"};
    ii::core::Observable<bool> failing{false}, visible{true};
    std::shared_ptr<SessionRecord> selected = std::make_shared<SessionRecord>();
    ii::runtime::QueryPort<SessionRead, std::int64_t> readPort{[this](const SessionRead&) {
        if (failing.get()) throw std::runtime_error("Requested query failure");
        return number.get();
    }};
    ii::runtime::QueryPort<SessionCommand> commandPort{[this](const SessionCommand&) {
        number.set(number.get() + 1);
    }};
    SessionRead readRequest() const { return {}; }
    const ii::runtime::QueryPort<SessionRead, std::int64_t>& queries() const { return readPort; }
    SessionCommand commandRequest() const { return {}; }
    std::shared_ptr<SessionRecord> record() const { return selected; }
    void incrementNumeric() { numericValue.set(numericValue.get() + 1); }
    void setNumeric(double value) {
        if (!std::isfinite(value) || std::floor(value) != value || value < -1000 || value > 1000)
            throw std::invalid_argument("Numeric value must be an integer in range");
        numericValue.set(static_cast<std::int64_t>(value));
    }
    void incrementSession() { sessionCount.set(sessionCount.get() + 1); }
    void invalidSessionField(const std::string&) { throw std::invalid_argument("Session count must be an integer"); }
    void renameSession() { sessionTitle.set(sessionTitle.get() + " updated"); }
};
