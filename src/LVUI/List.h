#pragma once

#include "Text.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {

inline constexpr std::size_t list_maximum_items=65536;
enum class ListErrorCode { invalid, not_found, stale, exhausted, wrong_thread, busy };
struct ListError final { ListErrorCode code; std::string message; };
template<class T>using ListResult=std::expected<T,ListError>;
struct ListEntry final {
    std::string key,label,description,icon_name;
    bool enabled=true,selectable=true;
    friend bool operator==(const ListEntry&,const ListEntry&)=default;
};
struct ListSnapshot final { std::vector<ListEntry> entries; std::uint64_t revision=0; };
class ListSource {
public:
    virtual ~ListSource()=default;
    [[nodiscard]] virtual ListResult<std::shared_ptr<const ListSnapshot>> snapshot()const=0;
};
enum class ListEditKind { insert, replace, erase, move };
struct ListEdit final {
    std::uint64_t revision=0;
    ListEditKind kind=ListEditKind::insert;
    std::string key,before;
    ListEntry item;
};
class ListEditor {
public:
    virtual ~ListEditor()=default;
    [[nodiscard]] virtual ListResult<std::uint64_t> apply(const ListEdit&)=0;
};

// A validated snapshot owns every key referenced by the index.
class ListIndex final {
public:
    [[nodiscard]] static ListResult<ListIndex> create(std::shared_ptr<const ListSnapshot> snapshot){
        if(!snapshot||snapshot->entries.size()>list_maximum_items)return error(ListErrorCode::invalid,"Invalid list snapshot size");
        ListIndex result;result.snapshot_=std::move(snapshot);result.positions_.reserve(result.snapshot_->entries.size());
        std::size_t bytes=0;
        for(std::size_t i=0;i<result.snapshot_->entries.size();++i){
            const auto& item=result.snapshot_->entries[i];
            if(item.key.empty()||item.key.size()>1024||item.icon_name.size()>256||!valid_accessibility_text(item.key)||
               !valid_accessibility_text(item.label)||!valid_accessibility_text(item.description)||!valid_accessibility_text(item.icon_name))
                return error(ListErrorCode::invalid,"Invalid list entry");
            const auto length=item.key.size()+item.label.size()+item.description.size()+item.icon_name.size();
            if(length>64*1024*1024-bytes)return error(ListErrorCode::invalid,"List text exceeds its memory limit");
            bytes+=length;if(!result.positions_.emplace(item.key,i).second)return error(ListErrorCode::invalid,"Duplicate list key");
        }
        return result;
    }
    [[nodiscard]] std::optional<std::size_t> find(std::string_view key)const{
        const auto found=positions_.find(key);return found==positions_.end()?std::nullopt:std::optional(found->second);
    }
    [[nodiscard]] std::shared_ptr<const ListSnapshot> snapshot()const{return snapshot_;}
private:
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    std::shared_ptr<const ListSnapshot> snapshot_;
    std::unordered_map<std::string_view,std::size_t> positions_;
};

// Edits publish one immutable snapshot only after revision and identity checks.
class ListMemory final : public ListSource,public ListEditor {
public:
    [[nodiscard]] static ListResult<std::shared_ptr<ListMemory>> create(std::vector<ListEntry> entries={}){
        auto initial=std::make_shared<const ListSnapshot>(ListSnapshot{std::move(entries),0});
        auto valid=ListIndex::create(initial);if(!valid)return std::unexpected(valid.error());
        return std::shared_ptr<ListMemory>(new ListMemory(std::move(initial)));
    }
    [[nodiscard]] ListResult<std::shared_ptr<const ListSnapshot>> snapshot()const override{
        std::scoped_lock lock(mutex_);return snapshot_;
    }
    [[nodiscard]] ListResult<std::uint64_t> apply(const ListEdit& edit)override{
        std::scoped_lock lock(mutex_);
        if(edit.revision!=snapshot_->revision)return error(ListErrorCode::stale,"List changed before the edit");
        if(unsigned(edit.kind)>unsigned(ListEditKind::move))return error(ListErrorCode::invalid,"Invalid list edit kind");
        auto entries=snapshot_->entries;
        const auto find=[&](std::string_view key){return std::ranges::find_if(entries,[&](const auto& item){return item.key==key;});};
        if(edit.kind==ListEditKind::insert){
            if(!edit.key.empty())return error(ListErrorCode::invalid,"List insertion uses the new item's key");
            auto destination=edit.before.empty()?entries.end():find(edit.before);
            if(!edit.before.empty()&&destination==entries.end())return error(ListErrorCode::not_found,"List insertion target was not found");
            entries.insert(destination,edit.item);
        }else{
            auto current=find(edit.key);if(current==entries.end())return error(ListErrorCode::not_found,"List item was not found");
            if(edit.kind==ListEditKind::replace){
                if(!edit.before.empty()||edit.item.key!=edit.key)return error(ListErrorCode::invalid,"List replacement must preserve its key");
                *current=edit.item;
            }else if(edit.kind==ListEditKind::erase){
                if(!edit.before.empty())return error(ListErrorCode::invalid,"List removal does not take a destination");entries.erase(current);
            }else{
                if(edit.before==edit.key)return snapshot_->revision;
                auto item=std::move(*current);entries.erase(current);
                auto destination=edit.before.empty()?entries.end():find(edit.before);
                if(!edit.before.empty()&&destination==entries.end())return error(ListErrorCode::not_found,"List move target was not found");
                entries.insert(destination,std::move(item));
            }
        }
        if(entries==snapshot_->entries)return snapshot_->revision;
        if(snapshot_->revision==std::numeric_limits<std::uint64_t>::max())return error(ListErrorCode::exhausted,"List revision exhausted");
        auto candidate=std::make_shared<const ListSnapshot>(ListSnapshot{std::move(entries),snapshot_->revision+1});
        if(auto valid=ListIndex::create(candidate);!valid)return std::unexpected(valid.error());
        snapshot_=std::move(candidate);return snapshot_->revision;
    }
private:
    explicit ListMemory(std::shared_ptr<const ListSnapshot> snapshot):snapshot_(std::move(snapshot)){}
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    mutable std::mutex mutex_;
    std::shared_ptr<const ListSnapshot> snapshot_;
};

// Selection is local to one view and never increments a document revision.
class ListController final {
public:
    [[nodiscard]] static ListResult<ListController> create(std::shared_ptr<const ListSource> source){
        if(!source)return error(ListErrorCode::invalid,"List source is required");
        auto snapshot=source->snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        auto index=ListIndex::create(std::move(*snapshot));if(!index)return std::unexpected(index.error());
        return ListController(std::move(source),std::move(*index));
    }
    [[nodiscard]] ListResult<std::shared_ptr<const ListSnapshot>> snapshot()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return index_.snapshot();
    }
    [[nodiscard]] ListResult<std::string> selection()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return selected_;
    }
    [[nodiscard]] ListResult<std::uint64_t> view_revision()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return revision_;
    }
    [[nodiscard]] ListResult<bool> select(std::string_view key){
        if(auto r=mutable_state();!r)return std::unexpected(r.error());
        if(!key.empty()){
            auto found=index_.find(key);if(!found)return error(ListErrorCode::not_found,"List selection was not found");
            const auto& item=index_.snapshot()->entries[*found];
            if(!item.enabled||!item.selectable)return error(ListErrorCode::invalid,"List item cannot be selected");
        }
        if(selected_==key)return false;
        if(revision_==std::numeric_limits<std::uint64_t>::max())return error(ListErrorCode::exhausted,"List view revision exhausted");
        selected_=std::string(key);++revision_;return true;
    }
    [[nodiscard]] ListResult<bool> refresh(){
        if(auto r=mutable_state();!r)return std::unexpected(r.error());
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{busy_};
        auto current=source_->snapshot();if(!current)return std::unexpected(current.error());
        if(!*current||(*current)->revision<index_.snapshot()->revision)return error(ListErrorCode::invalid,"List source revision moved backwards");
        if((*current)->revision==index_.snapshot()->revision)return false;
        auto next=ListIndex::create(std::move(*current));if(!next)return std::unexpected(next.error());
        bool clear=false;if(!selected_.empty()){
            auto position=next->find(selected_);clear=!position || !next->snapshot()->entries[*position].enabled || !next->snapshot()->entries[*position].selectable;
        }
        if(clear&&revision_==std::numeric_limits<std::uint64_t>::max())return error(ListErrorCode::exhausted,"List view revision exhausted");
        index_=std::move(*next);if(clear){selected_.clear();++revision_;}return true;
    }
private:
    ListController(std::shared_ptr<const ListSource> source,ListIndex index):source_(std::move(source)),index_(std::move(index)){}
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    [[nodiscard]] ListResult<void> thread()const{
        if(owner_!=std::this_thread::get_id())return error(ListErrorCode::wrong_thread,"List controller requires its owner thread");return {};
    }
    [[nodiscard]] ListResult<void> mutable_state()const{
        if(auto r=thread();!r)return r;if(busy_)return error(ListErrorCode::busy,"List controller is refreshing");return {};
    }
    std::shared_ptr<const ListSource> source_;
    ListIndex index_;
    std::string selected_;
    std::uint64_t revision_=0;
    const std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
};

} // namespace lvui
