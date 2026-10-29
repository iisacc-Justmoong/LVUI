#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <functional>
#include <utility>
struct NativeRepeatItem final {
    std::string id,title;
    std::vector<NativeRepeatItem> children;
    friend bool operator==(const NativeRepeatItem&,const NativeRepeatItem&)=default;
};
struct NativeRowActivation final {
    static inline int constructions=0,destructions=0;
    static inline bool fail_next=false;
    static inline std::function<void()> on_construct;
    NativeRowActivation(){if(fail_next){fail_next=false;throw std::runtime_error("Requested row activation failure");}++constructions;if(auto callback=std::exchange(on_construct,{}))callback();}
    ~NativeRowActivation(){++destructions;}
    ii::core::Observable<int> count{0};
    void increment(){count.set(count.get()+1);}
};
struct NativeRepetitionBackend final {
    ii::core::Observable<std::vector<NativeRepeatItem>> items{
        std::vector<NativeRepeatItem>{{"a","A",{}},{"b","B",{}},{"c","C",{}}}};
    ii::core::Observable<int> first{0},count{3};
};
struct NativeLiveItem final {
    static inline int constructions=0,destructions=0;
    std::string id;
    ii::core::Observable<std::string> title;
    ii::core::Observable<int> hits{0};
    NativeLiveItem(std::string key,std::string text):id(std::move(key)),title(std::move(text)){++constructions;}
    NativeLiveItem(const NativeLiveItem&)=delete;
    ~NativeLiveItem(){++destructions;}
    void hit(){hits.set(hits.get()+1);}
};
struct NativeVirtualCollectionBackend final {
    ii::core::Observable<std::vector<NativeRepeatItem>> items{[] {
        std::vector<NativeRepeatItem> result;
        for(int n=0;n<1000;++n)result.push_back({std::to_string(n),"Image "+std::to_string(n),{}});
        return result;
    }()};
    ii::core::Observable<int> width{480},height{170};
    ii::core::Observable<bool> horizontal{false};
};
struct NativeLiveCollectionBackend final {
    ii::core::Observable<std::vector<std::shared_ptr<NativeLiveItem>>> items{
        std::vector<std::shared_ptr<NativeLiveItem>>{std::make_shared<NativeLiveItem>("a","A"),std::make_shared<NativeLiveItem>("b","B")}};
};
struct NativeMeasuredCollectionBackend final {
    ii::core::Observable<std::vector<std::shared_ptr<NativeLiveItem>>> items{[] {
        std::vector<std::shared_ptr<NativeLiveItem>> result;
        for(int n=0;n<1000;++n)result.push_back(std::make_shared<NativeLiveItem>(std::to_string(n),n%3?"Short":std::string(120,'W')));
        return result;
    }()};
    ii::core::Observable<int> width{240},height{180},cell_width{0},spacing{2};
    ii::core::Observable<int> geometry_revision{0};
    ii::core::Observable<bool> measured{true},empty{false};
};
struct NativeNavigationBackend final {
    ii::core::Observable<std::vector<NativeRepeatItem>> items{[] {
        std::vector<NativeRepeatItem> result;
        for(int n=0;n<1000;++n)result.push_back({std::to_string(n),n==998?"Disabled":"Image "+std::to_string(n),{}});
        return result;
    }()};
    std::vector<std::string> focus_events,activations;
    std::vector<std::vector<std::string>> selection_events;
    ii::core::Observable<int> file_revision{0};
    ii::core::Observable<bool> select_on_click{true};
    int button_actions=0;
    static inline std::function<void()> on_activate;
    void focus_changed(std::string key){focus_events.push_back(std::move(key));}
    void selection_changed(std::vector<std::string> keys){selection_events.push_back(std::move(keys));}
    void activate(std::string key){activations.push_back(std::move(key));if(auto callback=std::exchange(on_activate,{}))callback();}
    void button_action(){++button_actions;}
};
