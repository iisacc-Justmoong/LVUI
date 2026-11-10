#pragma once

#include <LVUI/List.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <memory>
#include <string>
#include <utility>

struct NativeOwnedListDragModel final {
    std::shared_ptr<lvui::ListMemory> source=lvui::ListMemory::create({
        {.key="a",.label="Alpha"},{.key="b",.label="Beta"},{.key="c",.label="Gamma"}}).value();
    std::shared_ptr<lvui::ListController> controller=std::make_shared<lvui::ListController>(lvui::ListController::create(source).value());
    std::shared_ptr<lvui::ListEditor> editor=source;
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<bool> reorderable{true};
    std::size_t from=0,to=0;int moved_count=0;std::string key,before;
    void moved(std::string item,std::size_t old_index,std::size_t new_index,std::string before_key){
        ++moved_count;key=std::move(item);from=old_index;to=new_index;before=std::move(before_key);
        revision.set(std::int64_t(source->snapshot().value()->revision));
    }
};
