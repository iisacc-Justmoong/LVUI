#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <functional>
struct NativeReadonlyItem final {
    static inline int live=0;
    struct Metrics final {ii::core::Observable<int> count{2};};
    Metrics metrics;
    ii::core::Observable<std::string> title;
    explicit NativeReadonlyItem(std::string text):title(std::move(text)){++live;}
    ~NativeReadonlyItem(){--live;}
};
struct NativeReadonlyRecord final {std::string title;int count=0;};
struct NativeReadonlyModel final {
private:
    std::shared_ptr<NativeReadonlyItem> stable=std::make_shared<NativeReadonlyItem>("Stable");
public:
    static inline int live=0;
    std::shared_ptr<NativeReadonlyItem> first=std::make_shared<NativeReadonlyItem>("First");
    std::shared_ptr<NativeReadonlyItem> second=std::make_shared<NativeReadonlyItem>("Second");
    ii::core::Observable<std::shared_ptr<const NativeReadonlyItem>> selected{first};
    std::reference_wrapper<const NativeReadonlyItem> borrowed{*stable};
    const NativeReadonlyItem* pointer=stable.get();
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<int> file_revision{0};
    NativeReadonlyRecord record{"Record",4};
    NativeReadonlyModel(){++live;}~NativeReadonlyModel(){--live;}
    void selectSecond(){selected.set(second);}
    void editStable(int count){stable->metrics.count.set(count);}
};
