#pragma once
#include <array>
#include <functional>
#include <stdexcept>
#include <vector>

struct WindowCloseFixtureBackend final {
    inline static int created = 0, destroyed = 0, opened_count = 0;
    inline static std::array<bool, 8> allow{true, true, true, true, true, true, true, true};
    inline static bool fail = false;
    inline static std::function<void()> during_open;
    inline static std::function<void(int)> during_request;
    inline static std::vector<int> requests;
    const int index = created++;
    ~WindowCloseFixtureBackend() { ++destroyed; }
    void opened() { ++opened_count; if(during_open)during_open(); }
    bool can_close() {
        requests.push_back(index);
        if (fail) throw std::runtime_error("window close failure");
        const bool decision=allow.at(static_cast<std::size_t>(index));
        if(during_request)during_request(index);
        return decision;
    }
};
