#include <LVUI/List.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::ListResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
class InvalidSource final : public lvui::ListSource {
public:
    lvui::ListResult<std::shared_ptr<const lvui::ListSnapshot>> snapshot() const override {
        return std::make_shared<const lvui::ListSnapshot>(lvui::ListSnapshot{{{.key="same"},{.key="same"}},0});
    }
};
class ReentrantSource final : public lvui::ListSource {
public:
    std::shared_ptr<lvui::ListMemory> memory; lvui::ListController* controller=nullptr; mutable bool guarded=false;
    lvui::ListResult<std::shared_ptr<const lvui::ListSnapshot>> snapshot()const override {
        if(controller){auto nested=controller->select("b");guarded=!nested&&nested.error().code==lvui::ListErrorCode::busy;}
        return memory->snapshot();
    }
};

void incremental_rows()
{
    using namespace lvui;
    std::vector<ListEntry> rows;
    for (int i = 0; i < 4096; ++i)
        rows.push_back({.key = std::to_string(i), .label = "Original"});
    auto source = take(ListMemory::create(std::move(rows)));
    auto controller = take(ListController::create(source));
    const auto initial = take(controller.index());
    const auto untouched = &initial.entry(2047);
    require(take(controller.select("2047")), "Selecting a retained row failed");

    require(take(source->apply({.revision = 0, .kind = ListEditKind::replace, .key = "2048",
                              .item = {.key = "2048", .label = "Changed"}})) == 1,
            "The row edit was not published");
    require(take(controller.refresh()), "The controller missed the row edit");
    const auto edited = take(controller.index());
    require(initial.entry(2048).label == "Original" && edited.entry(2048).label == "Changed"
            && &edited.entry(2047) == untouched && take(controller.selection()) == "2047",
            "A row edit copied unrelated rows or changed a retained revision");

    require(take(source->apply({.revision = 1, .kind = ListEditKind::move, .key = "2047"})) == 2,
            "Moving a retained row failed");
    require(take(controller.refresh()), "The controller missed an order change");
    const auto moved = take(controller.index());
    require(moved.find("2047") == 4095 && &moved.entry(4095) == untouched
            && &moved.entry(2047) == &edited.entry(2048) && take(controller.selection()) == "2047",
            "Moving a row copied content or lost the selection");
}
}
int main(){
    using namespace lvui;
    incremental_rows();
    auto source=take(ListMemory::create({{.key="a",.label="Alpha"},{.key="b",.label="Beta"},{.key="c",.label="Gamma"}}));
    auto first=take(source->snapshot());auto controller=take(ListController::create(source));
    require(take(controller.select("a")) && take(controller.selection())=="a" && take(source->snapshot())->revision==0,
            "List selection changed the document or lost its key");
    require(take(source->apply({.revision=0,.kind=ListEditKind::move,.key="a",.before="c"}))==1 &&
            take(source->snapshot())->entries[1].key=="a" && first->entries.front().key=="a",
            "List move failed or changed a retained snapshot");
    auto stale=source->apply({.revision=0,.kind=ListEditKind::erase,.key="a"});
    require(!stale && stale.error().code==ListErrorCode::stale && take(controller.selection())=="a",
            "Stale list mutation overwrote data or changed the view");
    require(take(controller.refresh()) && take(controller.selection())=="a" && take(controller.snapshot())->revision==1,
            "List refresh lost a surviving selection");
    require(take(source->apply({.revision=1,.kind=ListEditKind::move,.key="a",.before="a"}))==1,
            "A no-op list move increased the revision");
    require(take(source->apply({.revision=1,.kind=ListEditKind::replace,.key="a",.item={.key="a",.label="Alpha",.enabled=false}}))==2 &&
            take(controller.refresh()) && take(controller.selection()).empty() && take(controller.view_revision())==2,
            "List refresh did not clear a disabled selection");
    require(!source->apply({.revision=2,.kind=ListEditKind::insert,.before="b",.item={.key="c",.label="Duplicate"}}) &&
            !source->apply({.revision=2,.kind=ListEditKind::replace,.key="b",.item={.key="new"}}) &&
            !source->apply({.revision=2,.kind=ListEditKind::move,.key="b",.before="missing"}) &&
            take(source->snapshot())->revision==2, "Invalid list edits changed the source");
    require(!ListController::create(std::make_shared<InvalidSource>()) &&
            !ListMemory::create({{.key="bad",.label=std::string("a\0b",3)}}), "Invalid list snapshots were accepted");
    std::atomic<int> success=0,conflicts=0;
    auto insert=[&](const char* key){auto result=source->apply({.revision=2,.kind=ListEditKind::insert,.item={.key=key,.label=key}});
        if(result)++success;else if(result.error().code==ListErrorCode::stale)++conflicts;};
    std::thread one(insert,"d"),two(insert,"e");one.join();two.join();
    require(success==1 && conflicts==1 && take(source->snapshot())->revision==3,
            "Concurrent list edits did not publish exactly one revision");
    auto reentrant=std::make_shared<ReentrantSource>();reentrant->memory=source;
    auto guarded=take(ListController::create(reentrant));reentrant->controller=&guarded;
    (void)take(source->apply({.revision=3,.kind=ListEditKind::insert,.item={.key="z",.label="Z"}}));
    require(take(guarded.refresh()) && reentrant->guarded,"A list source changed its controller during refresh");
    bool wrong=false;std::thread worker([&]{auto result=controller.select("b");wrong=!result&&result.error().code==ListErrorCode::wrong_thread;});
    worker.join();require(wrong,"List controller accepted a foreign-thread mutation");
}
