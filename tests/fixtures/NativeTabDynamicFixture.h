#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct NativeTabDynamicEntry final {
    std::string id;
    ii::core::Observable<std::string> label;
    ii::core::Observable<bool> visible{true},show{true};
    int clicks=0;
    NativeTabDynamicEntry(std::string key,std::string value):id(std::move(key)),label(std::move(value)){}
    void clicked(){++clicks;}
};
struct NativeTabDynamicModel final {
    using Entry=std::shared_ptr<NativeTabDynamicEntry>;
    Entry a=std::make_shared<NativeTabDynamicEntry>("a","Alpha");
    Entry b=std::make_shared<NativeTabDynamicEntry>("b","Beta");
    Entry c=std::make_shared<NativeTabDynamicEntry>("c","Gamma");
    ii::core::Observable<std::vector<Entry>> rows{std::vector<Entry>{}},groups{std::vector<Entry>{}};
    ii::core::Observable<bool> show{false},more{false};
    ii::core::Observable<std::string> title{"Bundle"};
    ii::core::Observable<double> spacing{4};
    ii::core::Observable<int> index{0},document_revision{0};
    std::vector<int> selections;
    void changed(int value){selections.push_back(value);index.set(value);}
};
