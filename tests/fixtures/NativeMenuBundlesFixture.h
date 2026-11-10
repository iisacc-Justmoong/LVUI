#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct NativeMenuBundleEntry final {
    std::string id;
    ii::core::Observable<std::string> label;
    ii::core::Observable<bool> visible{true},show{true};
    ii::core::Observable<double> height{18};
    int clicks=0;
    NativeMenuBundleEntry(std::string key,std::string value):id(std::move(key)),label(std::move(value)){}
    void clicked(){++clicks;}
};
struct NativeMenuBundlesModel final {
    using Entry=std::shared_ptr<NativeMenuBundleEntry>;
    Entry a=std::make_shared<NativeMenuBundleEntry>("a","Alpha");
    Entry b=std::make_shared<NativeMenuBundleEntry>("b","Beta");
    Entry c=std::make_shared<NativeMenuBundleEntry>("c","Gamma");
    ii::core::Observable<std::vector<Entry>> rows{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> conditional_rows{std::vector<Entry>{}};
    ii::core::Observable<bool> show{false},more{false};
    ii::core::Observable<std::string> title{"Bundle"};
    ii::core::Observable<double> spacing{2};
    ii::core::Observable<int> document_revision{0};
    std::vector<int> triggered;
    void on_triggered(int index){triggered.push_back(index);}
};
