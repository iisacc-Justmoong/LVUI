#include <LVUI/NativeSection.h>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
struct Item { int id; std::vector<int> layout_groups; };
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}
int main() {
    using namespace lvui::native;
    using Items = std::vector<Item>;
    Items published; bool reject = false;
    auto deliver = [&](Items values) -> ii::core::Result<void> {
        if (reject) return ii::core::fail(ii::core::ErrorCode::failed, "Rejected section");
        published = std::move(values); return {};
    };
    auto capture = std::make_shared<SectionCapture<Item>>();
    good(capture->add_group(3)); good(capture->append({1, {2}}));
    auto slot = take(capture->slot()); good(slot({Item{2, {}}})); good(capture->publish(deliver));
    require(published.size() == 2 && published[0].id == 1 && published[0].layout_groups == std::vector<int>({2, 3}) &&
            published[1].id == 2 && published[1].layout_groups == std::vector<int>({3}),
            "Section order or supplied groups were lost");
    reject = true; require(!slot({Item{3, {}}}) && published[1].id == 2, "Rejected section replaced live values");
    reject = false; good(slot({Item{4, {}}}));
    require(published[1].id == 4 && published[1].layout_groups == std::vector<int>({3}), "Section did not recover");
    good(capture->close()); require(!slot({Item{5, {}}}), "Retired section accepted an update");

    auto rows = std::make_shared<SectionRepeatedRows<Item>>(deliver);
    auto first = take(rows->row("first")); auto second = take(rows->row("second"));
    good(first->add_group(9)); good(first->append({5, {}})); good(second->append({6, {}}));
    good(rows->publish({{"first", first}, {"second", second}}));
    require(published.size() == 2 && published[0].id == 5 && published[0].layout_groups == std::vector<int>({9}) &&
            published[1].id == 6, "Repeated sections did not retain their ordered groups");
    reject = true; require(!first->replace({Item{7, {}}}) && published[0].id == 5, "Rejected row replaced live values");
    reject = false; good(first->replace({Item{7, {}}}));
    require(published[0].id == 7 && published[0].layout_groups == std::vector<int>({9}), "Repeated section did not recover");
    good(rows->publish({{"second", second}}));
    require(published.size() == 1 && published[0].id == 6 && !first->replace({Item{8, {}}}), "Removed row did not retire");
    good(rows->close()); require(!second->replace({Item{8, {}}}), "Closed repeated section accepted an update");
}
