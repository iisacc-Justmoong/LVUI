#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NativeInput {
struct Record final {std::string title;int count=0;};
struct Item final {
    static inline int live=0;
    std::string id;
    ii::core::Observable<std::string> title;
    ii::core::Observable<int> count{2};
    Item(std::string key,std::string text):id(std::move(key)),title(std::move(text)){++live;}
    ~Item(){--live;}
};
}
struct NativeObjectInputModel final {
private:
    std::shared_ptr<NativeInput::Item> stable=std::make_shared<NativeInput::Item>("stable","Stable");
public:
    static inline int live=0;
    std::shared_ptr<NativeInput::Item> first=std::make_shared<NativeInput::Item>("first","First");
    std::shared_ptr<NativeInput::Item> second=std::make_shared<NativeInput::Item>("second","Second");
    ii::core::Observable<std::shared_ptr<const NativeInput::Item>> selected{first};
    std::reference_wrapper<const NativeInput::Item> borrowed{*stable};
    const NativeInput::Item* pointer=stable.get();
    ii::core::Observable<NativeInput::Record> record{{"Record",4}};
    ii::core::Observable<int> count{3};
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<std::vector<std::shared_ptr<const NativeInput::Item>>> rows{{first,second}};
    ii::core::Observable<int> file_revision{0};
    int computations=0;
    explicit NativeObjectInputModel(bool invalid=false){++live;if(invalid)selected.set({});}~NativeObjectInputModel(){--live;}
    NativeInput::Record makeRecord(int value){++computations;return {"Computed "+std::to_string(value),value};}
    void editStable(int value){stable->count.set(value);}
};
