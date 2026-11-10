#pragma once
#include "NativeOwnedTableFixture.h"
#include <string>
#include <utility>

struct NativeOwnedTableActionBackend final {
    NativeOwnedTableModel data;
    ii::core::Observable<std::shared_ptr<lvui::TableController>> controller{data.controller.get()};
    std::string opened;
    int open_count=0;
    auto open(std::string key){return [this,key=std::move(key)]{opened=key;++open_count;};}
};
