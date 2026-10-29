#pragma once
#include <LVRS/HierarchyList.h>
struct NativeHierarchyBackend final {
    std::shared_ptr<lvrs::HierarchyModel> model;
    std::shared_ptr<lvrs::HierarchyController> controller;
    ii::core::Observable<std::shared_ptr<lvrs::HierarchyController>> source_controller;
    ii::core::Observable<double> row_height{20};
    ii::core::Observable<int> file_revision{0};
    int activations=0;
    template<class T>static T take(ii::core::Result<T> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}return std::move(*r);}
    NativeHierarchyBackend(){model=take(lvrs::HierarchyModel::create());take(model->set_rows({{.key="root",.label="Layers",.item_id=1,.expanded=true},{.key="group",.label="Group",.item_id=2,.depth=1},{.key="image",.label="Image",.item_id=3,.depth=2},{.key="other",.label="Other",.item_id=4}}));controller=take(lvrs::HierarchyController::create(model));source_controller.set(controller);}
    std::string active()const{return take(source_controller.get()->state()).active_key;}
    void select(std::string,std::int64_t,std::size_t){++activations;}
    void image(){take(source_controller.get()->activate_by_key("image"));}
    void compact(){row_height.set(17);}
    void replace(){auto next=take(lvrs::HierarchyModel::create());take(next->set_rows({{.key="root",.label="New root",.expanded=true},{.key="chosen",.label="Chosen",.depth=1,.selected=true}}));source_controller.set(take(lvrs::HierarchyController::create(next)));}
    void restore(){source_controller.set(controller);}
};
struct NativeHierarchyItemBackend final {
    ii::core::Observable<std::string> label{"Layer"};
    ii::core::Observable<std::int64_t> count{2};
    ii::core::Observable<bool> selected{false},expanded{false};
    int activations=0;
    void clicked(){++activations;selected.set(!selected.get());}
    void expand(bool value){expanded.set(value);}
    void rename(){label.set("Renamed");count.set(3);}
};
struct NativeHierarchyCountLifetime final {
    inline static int live=0,created=0;
    int ticket=++created;
    NativeHierarchyCountLifetime(){++live;}
    ~NativeHierarchyCountLifetime(){--live;}
};
struct NativeHierarchyCountBackend final {
    ii::core::Observable<std::string> label{"Layer"};
    ii::core::Observable<std::int64_t> count{2};
    int activations=0;
    bool hide_on_click=false;
    int live_during_hide=-1;
    std::string describe(const lvrs::HierarchyItem& item)const{auto value=item.options();if(!value)throw std::runtime_error(value.error().message);return value->row.label+":"+std::to_string(value->row.count);}
    std::string self(const NativeHierarchyCountBackend& other)const{if(this!=&other)throw std::runtime_error("Backend argument was copied");return "Same owner";}
    void clicked(){++activations;if(hide_on_click){count.set(-1);live_during_hide=NativeHierarchyCountLifetime::live;}}
    void rename(){label.set("Renamed");count.set(3);}
    void hide(){count.set(-1);}
    void show(){count.set(2);}
};
struct NativeHierarchyDelegateLifetime final {
    inline static int live=0,created=0;
    NativeHierarchyDelegateLifetime(){++live;++created;}
    ~NativeHierarchyDelegateLifetime(){--live;}
};
struct NativeHierarchyDelegateBackend final {
    std::shared_ptr<lvrs::HierarchyModel> model;
    std::shared_ptr<lvrs::HierarchyController> controller;
    ii::core::Observable<std::shared_ptr<lvrs::HierarchyController>> source_controller;
    ii::core::Observable<std::string> prefix{"View "};
    ii::core::Observable<double> padding{4};
    ii::core::Observable<bool> bad_label{false};
    int activations=0,count_actions=0;std::string last_key;
    template<class T>static T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
    NativeHierarchyDelegateBackend(){model=take(lvrs::HierarchyModel::create());std::vector<lvrs::HierarchyRow> rows;for(int i=0;i<100;++i)rows.push_back({.key="k"+std::to_string(i),.label="Layer "+std::to_string(i)});take(model->set_rows(std::move(rows)));controller=take(lvrs::HierarchyController::create(model));source_controller.set(controller);}
    std::string title(const lvrs::HierarchyDelegateContext& entry)const{if(bad_label.get())throw std::runtime_error("Rejected delegate projection");return prefix.get()+take(entry.options()).row.label;}
    std::string active()const{return take(source_controller.get()->state()).active_key;}
    void clicked(){++activations;}
    ii::runtime::Task action(const lvrs::HierarchyItem& row){auto key=take(row.options()).row.key;return [this,key=std::move(key)]{++count_actions;last_key=key;};}
    void replace(){auto next=take(lvrs::HierarchyModel::create());take(next->set_rows({{.key="new",.label="New"}}));source_controller.set(take(lvrs::HierarchyController::create(next)));}
};
