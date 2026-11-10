#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct NativeRepeatedMenuEntry final {
    std::string id;
    ii::core::Observable<std::string> label;
    ii::core::Observable<bool> visible{true}, enabled{true}, show{true}, alternate{false};
    ii::core::Observable<double> height{18};
    int clicks=0;
    NativeRepeatedMenuEntry(std::string key,std::string text):id(std::move(key)),label(std::move(text)){}
    void clicked(){++clicks;}
};
struct NativeMenuBranchLocal final {
    static inline int constructions=0,destructions=0;
    static inline bool fail_next=false;
    static inline NativeMenuBranchLocal* latest=nullptr;
    ii::core::Observable<std::string> label{"Local action"};
    NativeMenuBranchLocal(){if(std::exchange(fail_next,false))throw std::runtime_error("Requested local activation failure");++constructions;latest=this;}
    ~NativeMenuBranchLocal(){if(latest==this)latest=nullptr;++destructions;}
};
struct NativeRepeatedMenuModel final {
    using Entry=std::shared_ptr<NativeRepeatedMenuEntry>;
    Entry a=std::make_shared<NativeRepeatedMenuEntry>("a","Alpha");
    Entry b=std::make_shared<NativeRepeatedMenuEntry>("b","Beta");
    Entry c=std::make_shared<NativeRepeatedMenuEntry>("c","Gamma");
    Entry d=std::make_shared<NativeRepeatedMenuEntry>("d","Delta");
    Entry x=std::make_shared<NativeRepeatedMenuEntry>("x","Separator X");
    Entry y=std::make_shared<NativeRepeatedMenuEntry>("y","Separator Y");
    ii::core::Observable<std::vector<Entry>> items{std::vector<Entry>{a,b,c}};
    ii::core::Observable<std::vector<Entry>> separators{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> nested{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> conditional{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> optional{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> custom{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> custom_nested{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> custom_direct{std::vector<Entry>{}};
    ii::core::Observable<std::vector<Entry>> custom_direct_nested{std::vector<Entry>{}};
    ii::core::Observable<bool> direct_show{false},direct_custom{false};
    ii::core::Observable<double> spacing{2};
    ii::core::Observable<int> first{0},count{4},file_revision{0};
    std::vector<int> triggered;
    int direct_clicks=0;
    void on_triggered(int index){triggered.push_back(index);}
    void direct_clicked(){++direct_clicks;}
};
