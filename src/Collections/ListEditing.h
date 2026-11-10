#pragma once

#include <LVUI/List.h>

#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui {

struct ListEditState final {
    ListEntry original;
    std::string draft;
    std::size_t source_index=0;
    std::uint64_t source_revision=0;
};

// A session owns a draft; only its injected editor may mutate source data.
// Copies share one session and cannot submit the same edit more than once.
class ListEditing final {
    struct Data final {
        std::shared_ptr<ListEditor> editor;
        ListEditState state;
        const std::thread::id owner=std::this_thread::get_id();
        bool active=true,committing=false;
    };
public:
    [[nodiscard]] static ListResult<ListEditing> begin(std::shared_ptr<const ListSnapshot> snapshot,
            std::shared_ptr<ListEditor> editor,std::string_view key){
        if(!editor)return error(ListErrorCode::invalid,"List editor is required");
        auto index=ListIndex::create(std::move(snapshot));if(!index)return std::unexpected(index.error());
        auto position=index->find(key);if(!position)return error(ListErrorCode::not_found,"List edit row was not found");
        const auto data=index->snapshot();const auto& entry=data->entries[*position];
        if(!entry.enabled||!entry.editable)return error(ListErrorCode::invalid,"List row cannot be edited");
        return ListEditing(std::make_shared<Data>(std::move(editor),ListEditState{entry,entry.label,*position,data->revision}));
    }
    [[nodiscard]] ListResult<ListEditState> state()const{
        auto data=data_;if(!data)return error(ListErrorCode::invalid,"List edit has ended");
        if(auto valid=ready(*data);!valid)return std::unexpected(valid.error());return data->state;
    }
    [[nodiscard]] ListResult<bool> set_draft(std::string value){
        auto data=data_;if(!data)return error(ListErrorCode::invalid,"List edit has ended");
        if(auto valid=ready(*data,true);!valid)return std::unexpected(valid.error());
        if(!valid_accessibility_text(value))return error(ListErrorCode::invalid,"Invalid list edit draft");
        if(data->state.draft==value)return false;data->state.draft=std::move(value);return true;
    }
    [[nodiscard]] ListResult<std::uint64_t> commit(){
        auto data=data_;if(!data)return error(ListErrorCode::invalid,"List edit has ended");
        if(auto valid=ready(*data,true);!valid)return std::unexpected(valid.error());
        auto item=data->state.original;item.label=data->state.draft;
        data->committing=true;struct Reset final{bool& flag;~Reset(){flag=false;}}reset{data->committing};
        auto result=data->editor->apply({.revision=data->state.source_revision,.kind=ListEditKind::replace,
                                         .key=item.key,.item=std::move(item)});
        if(result)data->active=false;return result;
    }
    [[nodiscard]] ListResult<void> cancel(){
        auto data=data_;if(!data)return error(ListErrorCode::invalid,"List edit has ended");
        if(auto valid=ready(*data,true);!valid)return std::unexpected(valid.error());data->active=false;return {};
    }
private:
    explicit ListEditing(std::shared_ptr<Data> data):data_(std::move(data)){}
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    [[nodiscard]] static ListResult<void> ready(const Data& data,bool mutation=false){
        if(data.owner!=std::this_thread::get_id())return error(ListErrorCode::wrong_thread,"List edit requires its owner thread");
        if(mutation&&data.committing)return error(ListErrorCode::busy,"List edit is committing");
        if(!data.active)return error(ListErrorCode::invalid,"List edit has ended");return {};
    }
    std::shared_ptr<Data> data_;
};

} // namespace lvui
