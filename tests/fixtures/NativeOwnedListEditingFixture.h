#pragma once

#include <LVUI/List.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <memory>
#include <string>
#include <utility>

struct NativeOwnedListEditingModel final {
    std::shared_ptr<lvui::ListMemory> source=lvui::ListMemory::create({
        {.key="a",.label="Alpha"},{.key="locked",.label="Locked",.editable=false}}).value();
    std::shared_ptr<lvui::ListController> controller=std::make_shared<lvui::ListController>(lvui::ListController::create(source).value());
    std::shared_ptr<lvui::ListEditor> editor=source;
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<bool> editable{true};
    int edited_count=0,submitted_count=0,rejected_count=0;
    std::string key,field,text,error;
    void edited(std::size_t,const lvui::ListEntry& entry,std::string field_name,std::string value){
        ++edited_count;key=entry.key;field=std::move(field_name);text=std::move(value);
    }
    void submitted(std::size_t,const lvui::ListEntry& entry,std::string value){
        ++submitted_count;key=entry.key;text=std::move(value);revision.set(std::int64_t(source->snapshot().value()->revision));
    }
    void rejected(std::size_t,const lvui::ListEntry& entry,std::string value,const lvui::ListError& failure){
        ++rejected_count;key=entry.key;text=std::move(value);error=failure.message;
    }
};
