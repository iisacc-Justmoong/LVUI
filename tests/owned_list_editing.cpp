#include <LVUI/ListEditing.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::ListResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
class ValidatingEditor final:public lvui::ListEditor {
public:
    explicit ValidatingEditor(std::shared_ptr<lvui::ListMemory> source):source_(std::move(source)){}
    lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit& edit)override{
        ++calls;if(edit.item.label=="forbidden")return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid,"Name rejected"});
        return source_->apply(edit);
    }
    int calls=0;
private:
    std::shared_ptr<lvui::ListMemory> source_;
};
class ReentrantEditor final:public lvui::ListEditor {
public:
    std::shared_ptr<lvui::ListMemory> source;lvui::ListEditing* editing=nullptr;bool guarded=false;
    lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit& edit)override{
        auto again=editing->commit();auto draft=editing->set_draft("wrong");auto cancel=editing->cancel();
        guarded=!again&&again.error().code==lvui::ListErrorCode::busy&&!draft&&draft.error().code==lvui::ListErrorCode::busy&&
                !cancel&&cancel.error().code==lvui::ListErrorCode::busy;
        return source->apply(edit);
    }
};
}
int main(){
    using namespace lvui;
    auto source=take(ListMemory::create({{.key="a",.label="Alpha",.description="Description",.icon_name="image"},
        {.key="locked",.label="Locked",.editable=false},{.key="disabled",.label="Disabled",.enabled=false}}));
    auto editor=std::make_shared<ValidatingEditor>(source);auto original=take(source->snapshot());
    require(!ListEditing::begin(original,{},"a")&&!ListEditing::begin(original,editor,"absent")&&
            !ListEditing::begin(original,editor,"locked")&&!ListEditing::begin(original,editor,"disabled"),
            "A missing, read-only, disabled or editorless row accepted editing");
    auto edit=take(ListEditing::begin(original,editor,"a"));auto alias=edit;auto state=take(edit.state());
    require(state.original==original->entries[0]&&state.source_index==0&&state.source_revision==0&&state.draft=="Alpha",
            "A list edit lost its source entry or revision");
    require(take(edit.set_draft("forbidden"))&&take(source->snapshot())->revision==0,
            "A list draft changed its source");
    auto rejected=edit.commit();require(!rejected&&take(edit.state()).draft=="forbidden"&&take(source->snapshot())->revision==0,
            "A rejected list edit lost its draft or changed its source");
    require(take(edit.set_draft("Renamed"))&&!take(edit.set_draft("Renamed"))&&take(edit.commit())==1&&!edit.commit()&&!alias.commit(),
            "A list edit did not commit exactly once");
    auto renamed=take(source->snapshot());require(renamed->entries[0].label=="Renamed"&&renamed->entries[0].description=="Description"&&
        renamed->entries[0].icon_name=="image"&&original->entries[0].label=="Alpha","A rename lost other row fields or mutated its old snapshot");
    auto no_change=take(ListEditing::begin(renamed,editor,"a"));require(take(no_change.commit())==1&&take(source->snapshot())->revision==1,
        "An unchanged name created a source revision");
    auto stale=take(ListEditing::begin(renamed,editor,"a"));require(take(stale.set_draft("Pending")),"Stale edit setup failed");
    auto changed=renamed->entries[0];changed.label="Newer";
    require(take(source->apply({.revision=1,.kind=ListEditKind::replace,.key="a",.item=changed}))==2,"Concurrent list change failed");
    auto conflict=stale.commit();require(!conflict&&conflict.error().code==ListErrorCode::stale&&take(stale.state()).draft=="Pending"&&
        take(source->snapshot())->entries[0].label=="Newer","A stale list edit overwrote newer data or lost its draft");
    require(bool(stale.cancel())&&!stale.state()&&!stale.commit(),"A canceled edit remained usable");
    auto isolated=take(ListEditing::begin(take(source->snapshot()),editor,"a"));bool wrong=false;
    std::thread worker([&]{auto result=isolated.set_draft("Other");wrong=!result&&result.error().code==ListErrorCode::wrong_thread;});worker.join();
    require(wrong&&!isolated.set_draft(std::string("bad\xff",4))&&!isolated.set_draft(std::string(65537,'a')),
        "A list edit accepted a foreign thread, invalid UTF-8 or an oversized draft");
    auto moved=std::move(isolated);require(!isolated.state()&&bool(moved.cancel()),"A moved edit did not retain its sole session");
    auto reentrant=std::make_shared<ReentrantEditor>();reentrant->source=source;
    auto guarded=take(ListEditing::begin(take(source->snapshot()),reentrant,"a"));reentrant->editing=&guarded;
    require(take(guarded.set_draft("Final"))&&take(guarded.commit())==3&&reentrant->guarded&&
            take(source->snapshot())->entries[0].label=="Final","A reentrant list editor changed the pending edit");
}
