#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <stdexcept>
#include <string>
#include <vector>
struct NativeConditionalMenuModel final {
    ii::core::Observable<bool> show{false},showDivider{false},itemVisible{true},enabled{true},primary{false},third{false},local{false},group{false},groupDetail{false},plain{false},tab{false},shadow{false};
    ii::core::Observable<double> height{18},alternate_height{18};
    ii::core::Observable<std::string> label{"Conditional"};
    ii::core::Observable<int> file_revision{0};
    std::vector<int> triggered_indices;
    int clicks=0;
    void triggered(int value){triggered_indices.push_back(value);}
    void clicked(){++clicks;}
};
struct NativeConditionalMenuBranch final {
    inline static int constructions=0,destructions=0;
    inline static bool reject=false;
    inline static NativeConditionalMenuBranch* live=nullptr;
    ii::core::Observable<std::string> title{"Local action"};
    NativeConditionalMenuBranch(){if(reject)throw std::runtime_error("branch rejected");++constructions;live=this;}
    ~NativeConditionalMenuBranch(){++destructions;if(live==this)live=nullptr;}
    NativeConditionalMenuBranch(const NativeConditionalMenuBranch&)=delete;
    NativeConditionalMenuBranch& operator=(const NativeConditionalMenuBranch&)=delete;
};
struct NativeConditionalMenuGroup final {
    inline static NativeConditionalMenuGroup* live=nullptr;
    ii::core::Observable<std::string> title{"Grouped"};
    NativeConditionalMenuGroup(){live=this;}
    ~NativeConditionalMenuGroup(){if(live==this)live=nullptr;}
    NativeConditionalMenuGroup(const NativeConditionalMenuGroup&)=delete;
    NativeConditionalMenuGroup& operator=(const NativeConditionalMenuGroup&)=delete;
};
