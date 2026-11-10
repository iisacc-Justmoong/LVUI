#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Runtime/QueryPeer.h>
#include <cstdint>

struct NativeQueryPeerBackend final {
    ii::core::Observable<std::int64_t> received{0};
    ii::runtime::QueryPeer<std::int64_t, std::int64_t> peer{
        [this](const std::int64_t& value) { received.set(value); return value + 1; }};
    std::int64_t request() const { return 7; }
};
