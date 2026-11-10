#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Runtime/QueryPeer.h>
#include <memory>
#include <functional>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>

struct ImmediateDeclarationBackend final {
    struct Item final {
        int id;
        bool enabled;
        std::function<void()>* on_copy = nullptr;
        Item(int identifier, bool is_enabled) : id(identifier), enabled(is_enabled) {}
        Item(const Item& other) : id(other.id), enabled(other.enabled), on_copy(other.on_copy) {
            if (on_copy && *on_copy) (*on_copy)();
        }
        Item& operator=(const Item&) = default;
    };
    static std::shared_ptr<const ii::gui::RasterImage> makeIcon() {
        auto result = ii::gui::rgba_image(1, 1, {255, 255, 255, 255});
        if (!result) throw std::runtime_error(result.error().message);
        return *result;
    }
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{makeIcon()};
    ii::core::Observable<int> increment{1};
    int clicks = 0;
    int reads = 0;
    int factories = 0;
    int opened = 0;
    int after_retire = 0;
    int item_queries = 0;
    int query_calls = 0;
    int computed = 0;
    int chosen = 0;
    double last_query = 0;
    bool last_open = false;
    bool compared = true;
    std::vector<Item> items{{1, true}, {2, false}, {3, true}, {4, true}};
    std::vector<int> recorded;
    std::vector<int> evaluation_order;
    std::pair<int, int> recorded_pair{};
    std::function<void()> retire_action;
    std::function<void()> on_item_copy;
    ii::runtime::QueryPort<double, double> port{[this](const double& value) { ++query_calls; last_query = value; return value; }};
    std::string randomString() { ++reads; return "Action"; }
    void activate() { ++clicks; }
    void add(int value) { clicks += value; }
    auto makeHandler(int value) { ++factories; return [this, value] { clicks += value; }; }
    void didOpen(bool value) { ++opened; last_open = value; }
    std::span<const Item> readItems() { ++item_queries; return items; }
    std::span<const Item> retireItems() { retire(); return items; }
    void record(int value) { recorded.push_back(value); if (value == 1) items.clear(); }
    int next(int value) { evaluation_order.push_back(value); return value; }
    ii::runtime::QueryPort<double, double>& queryPort() { evaluation_order.push_back(7); return port; }
    void recordPair(int first, int second) { recorded_pair = {first, second}; }
    bool greater(int first, int second) const { return first > second; }
    const Item& pick(int, int) const { return items.front(); }
    void saveChosen(int value) { chosen = value; }
    void saveComputation(int value) { computed = value; }
    void saveComparison(bool value) { compared = value; }
    void retire() { if (retire_action) retire_action(); }
    bool retireAndTrue() { retire(); return true; }
    bool afterRetireTrue() { ++after_retire; return true; }
    int retireAndReturnInt() { retire(); return 1; }
    int afterRetireValue() { ++after_retire; return 2; }
    void afterRetire() { ++after_retire; }
};
