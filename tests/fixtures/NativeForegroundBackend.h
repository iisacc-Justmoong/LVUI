#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

struct NativeForegroundBackend final {
    inline static std::vector<std::string> events;
    inline static std::function<void()> during_foreground, during_background, during_close, during_files;
    inline static bool allow_close = true, fail_background = false;

    void start() { events.emplace_back("start"); }
    void ready() { events.emplace_back("ready"); }
    void stop() { events.emplace_back("stop"); }
    void active() { events.emplace_back("active"); }
    void inactive() { events.emplace_back("inactive"); }
    void foreground() { events.emplace_back("foreground"); if (during_foreground) during_foreground(); }
    void background() {
        events.emplace_back("background");
        if (during_background) during_background();
        if (fail_background) throw std::runtime_error("background callback failure");
    }
    void files(const std::vector<std::string>&) { events.emplace_back("files"); if (during_files) during_files(); }
    bool can_close() { events.emplace_back("close"); if (during_close) during_close(); return allow_close; }
};
