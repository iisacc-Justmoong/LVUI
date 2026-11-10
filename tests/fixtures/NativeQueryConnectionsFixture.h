#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Runtime/QueryStream.h>
#include <cstdint>
#include <optional>
#include <vector>

struct NativeQueryMessage final {
    std::int64_t number = 0;
    bool remove = false;
};

struct NativeQueryBackend final {
    ii::core::Observable<std::int64_t> selection{0};
    ii::core::Observable<bool> visible{true};
    ii::runtime::QueryStream<std::int64_t, NativeQueryMessage, NativeQueryMessage> events{
        [](std::int64_t selected, const NativeQueryMessage& message) -> std::optional<NativeQueryMessage> {
            if (message.number % 2 != selected) return {};
            return message;
        }};
    std::vector<std::int64_t> received;
    int refreshes = 0;

    auto& stream() { return events; }
    void changed(const NativeQueryMessage& message) {
        received.push_back(message.number);
        if (message.remove) visible.set(false);
    }
    void refresh() { ++refreshes; }
};
