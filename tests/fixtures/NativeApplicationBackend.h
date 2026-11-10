#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

struct NativeApplicationBackend final {
    inline static int created = 0;
    inline static int destroyed = 0;
    inline static std::vector<std::string> events;
    inline static std::vector<std::string> scene_events;
    inline static std::vector<std::vector<std::string>> file_events;
    inline static std::vector<std::vector<std::string>> launch_events;
    inline static bool fail_start = false, fail_ready = false, fail_stop = false, fail_active = false;
    inline static bool fail_files = false;
    inline static bool fail_launch = false;
    inline static bool allow_close_request = true, fail_close_request = false;
    inline static bool stopped_after_window = false;
    inline static std::function<void()> during_start, during_ready, during_stop, during_active, during_inactive;
    inline static std::function<void()> during_files;
    inline static std::function<void()> during_launch;
    inline static std::function<void()> during_close_request;
    inline static std::function<void(NativeApplicationBackend&)> during_scene_close;
    explicit NativeApplicationBackend(std::string value = "Native") : title(std::move(value)) { ++created; }
    ~NativeApplicationBackend() { ++destroyed; }
    NativeApplicationBackend(const NativeApplicationBackend&) = delete;
    NativeApplicationBackend& operator=(const NativeApplicationBackend&) = delete;
    void started();
    void ready();
    void stopped();
    void activated();
    void inactivated();
    bool can_close() {
        events.emplace_back("close-request");
        title = "Close requested";
        if (during_close_request) during_close_request();
        if (fail_close_request) throw std::runtime_error("close request failure");
        return allow_close_request;
    }
    void launch_arguments(const std::vector<std::string>& values) {
        launch_events.push_back(values); events.emplace_back("arguments");
        if (values.size() > 1) title = values[1];
        if (during_launch) during_launch();
        if (fail_launch) throw std::runtime_error("launch callback failure");
    }
    void open_files(const std::vector<std::string>& files) {
        file_events.push_back(files); events.emplace_back("files");
        if (!files.empty()) title = files.front();
        if (during_files) during_files();
        if (fail_files) throw std::runtime_error("file callback failure");
    }
    void scene_opened() { scene_events.emplace_back("opened"); }
    void scene_closed() { scene_events.emplace_back("closed"); if (during_scene_close) during_scene_close(*this); }
    std::string title;
    std::string accent = "green";
};

struct NativeWindowBackend final {
    inline static int created = 0;
    inline static int destroyed = 0;
    inline static std::vector<std::string> scene_events;
    inline static bool fail_open = false, fail_close = false;
    inline static std::function<void()> during_open, during_close;
    NativeWindowBackend() : sequence(++created) {}
    ~NativeWindowBackend() { ++destroyed; }
    NativeWindowBackend(const NativeWindowBackend&) = delete;
    NativeWindowBackend& operator=(const NativeWindowBackend&) = delete;
    void opened() {
        scene_events.push_back("open:" + std::to_string(sequence));
        if (during_open) during_open();
        if (fail_open) throw std::runtime_error("window open failure");
    }
    void closed() {
        scene_events.push_back("close:" + std::to_string(sequence));
        if (during_close) during_close();
        if (fail_close) throw std::runtime_error("window close failure");
    }
    int sequence;
    std::string title = "Window";
};

inline void NativeApplicationBackend::started() {
    events.emplace_back("start");
    if (during_start) during_start();
    if (fail_start) throw std::runtime_error("start callback failure");
}
inline void NativeApplicationBackend::ready() {
    events.emplace_back("ready");
    if (during_ready) during_ready();
    if (fail_ready) throw std::runtime_error("ready callback failure");
    title = "Ready app state";
}
inline void NativeApplicationBackend::stopped() {
    events.emplace_back("stop");
    if (during_stop) during_stop();
    stopped_after_window = NativeWindowBackend::created == NativeWindowBackend::destroyed;
    if (fail_stop) throw std::runtime_error("stop callback failure");
}
inline void NativeApplicationBackend::activated() {
    events.emplace_back("active");
    if (during_active) during_active();
    if (fail_active) throw std::runtime_error("active callback failure");
}
inline void NativeApplicationBackend::inactivated() {
    events.emplace_back("inactive");
    if (during_inactive) during_inactive();
}
