#include <LVUI/NativeQueryConnections.h>
#include <chrono>
#include <memory>
#include <optional>

int main() {
    using namespace std::chrono_literals;
    ii::runtime::EventLoop loop;
    auto registry = lvui::native::QueryConnections::create(loop);
    if (!registry) return 1;
    ii::runtime::QueryStream<int, int, int> stream([](int selection, int value) -> std::optional<int> {
        if (selection != value) return {};
        return value;
    });
    auto owner = std::make_shared<int>(0);
    auto connection = (*registry)->connect(owner, stream, 4, [owner](const int& value) { *owner = value; });
    if (!connection) return 2;
    (*connection)->activate();
    if (!stream.publish(4)) return 3;
    loop.poll({8, 1s});
    if (*owner != 4 || !(*registry)->retire(*connection)) return 4;
    auto closed = (*registry)->close();
    if (!closed || !*closed || !stream.close()) return 5;
    return 0;
}
