#pragma once
#include "NativeValues.h"
#include <algorithm>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <map>
#include <thread>
#include <unordered_set>

namespace lvui::native {
inline constexpr std::size_t maximum_collection_items=65536,maximum_collection_key_bytes=16*1024*1024;
template<class T> std::string collection_key(const T& value){
    std::string result;
    if constexpr(String<const T&>)result=std::string(lvui::native::string_view(value));
    else if constexpr(std::integral<T>&&!std::same_as<std::remove_cvref_t<T>,bool>)result=std::to_string(value);
    else if constexpr(Numeric<T>)result=std::to_string(integer(value));
    else static_assert(sizeof(T)==0,"A collection key requires String or an exactly representable integer");
    if(result.empty()||result.size()>1024||!ii::core::valid_utf8(result)||result.find('\0')!=result.npos)
        throw std::invalid_argument("Collection keys require nonempty UTF-8 up to 1024 bytes without NUL");
    return result;
}
struct CollectionOptions final {
    std::size_t first=0,count=std::numeric_limits<std::size_t>::max();
    double spacing=0;
    bool horizontal=false;
    bool sparse=false; // Mounted positions may skip invisible zero-height rows.
    friend bool operator==(const CollectionOptions&,const CollectionOptions&)=default;
};
template<class T> struct CollectionFrame final {
    struct Item final {
        std::string key;
        T value;
        std::size_t index;
        friend bool operator==(const Item&,const Item&)=default;
    };
    std::size_t total=0;
    CollectionOptions options{.count=0};
    std::vector<Item> items;
    friend bool operator==(const CollectionFrame&,const CollectionFrame&)=default;
};
template<class Range>
using collection_item_t=detail::StoredValue<std::ranges::range_reference_t<const std::remove_cvref_t<Range>>>;
template<class Range,class Key>
    requires std::ranges::forward_range<const Range>
auto collection_frame(const Range& values,Key key,CollectionOptions options={}){
    using T=collection_item_t<Range>;
    CollectionFrame<T> result;
    if(!std::isfinite(options.spacing)||options.spacing<0||options.spacing>32767)
        throw std::invalid_argument("Collection spacing must be finite and within 0..32767");
    if constexpr(std::ranges::sized_range<const Range>)
        if(std::ranges::size(values)>maximum_collection_items)throw std::length_error("Collection item limit is 65536");
    result.options=options;
    std::unordered_set<std::string> unique;
    std::size_t key_bytes=0;
    for(const auto& value:values){
        const auto index=result.total++;
        if(index==maximum_collection_items)throw std::length_error("Collection item limit is 65536");
        auto identity=collection_key(std::invoke(key,value,index));
        if(identity.size()>maximum_collection_key_bytes-key_bytes)
            throw std::length_error("Collection keys exceed 16 MiB");
        key_bytes+=identity.size();
        if(!unique.insert(identity).second)throw std::invalid_argument("Duplicate collection key");
        if(index>=options.first&&result.items.size()<options.count)
            result.items.push_back({std::move(identity),value_copy(value),index});
    }
    result.options.first=std::min(options.first,result.total);
    result.options.count=result.items.size();
    return result;
}
template<class T> struct CollectionRowValue final {
    T value;
    std::size_t index;
    friend bool operator==(const CollectionRowValue&,const CollectionRowValue&)=default;
};
template<class T> class KeyedRows final : public std::enable_shared_from_this<KeyedRows<T>> {
public:
    struct Row final {
        ii::core::Observable<CollectionRowValue<T>> state;
        ii::core::Observable<std::uint64_t> revision{0};
        Row(const T& value,std::size_t index):state(CollectionRowValue<T>{value,index}){}
    };
    struct Mount final {std::string key;std::shared_ptr<Row> row;};
    KeyedRows()=default;
    KeyedRows(const KeyedRows&)=delete;
    KeyedRows& operator=(const KeyedRows&)=delete;
    [[nodiscard]] std::shared_ptr<Row> find(std::string_view key)const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection requires its owner thread");
        const auto found=rows_.find(key);return found==rows_.end()?nullptr:found->second;
    }
    [[nodiscard]] std::size_t size()const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection requires its owner thread");
        return rows_.size();
    }
    [[nodiscard]] ii::core::Result<void> close(){
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Collection requires its owner thread");
        active_=false;rows_.clear();return {};
    }
    template<class Apply> [[nodiscard]] ii::core::Result<void> apply(const CollectionFrame<T>& frame,Apply apply){
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Collection requires its owner thread");
        if(!active_)return ii::core::fail(ii::core::ErrorCode::closed,"Collection is closed");
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Collection is already applying");
        if(frame.total>maximum_collection_items||frame.options.first>frame.total||frame.options.count!=frame.items.size()||
            frame.items.size()>frame.total-frame.options.first||!std::isfinite(frame.options.spacing)||frame.options.spacing<0||frame.options.spacing>32767)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid collection frame");
        [[maybe_unused]] auto admitted=this->weak_from_this().lock();
        updating_=true;
        struct Reset final{bool& flag;~Reset(){flag=false;}}reset{updating_};
        try{
            std::map<std::string,std::shared_ptr<Row>,std::less<>> next;
            std::vector<Mount> mounted;
            std::vector<CollectionRowValue<T>> updates;
            mounted.reserve(frame.items.size());updates.reserve(frame.items.size());
            for(std::size_t n=0;n<frame.items.size();++n){
                const auto& item=frame.items[n];
                const bool position=frame.options.sparse?
                    item.index<frame.total&&(n?item.index>frame.items[n-1].index:item.index==frame.options.first):item.index==frame.options.first+n;
                if(!position||collection_key(item.key)!=item.key||next.contains(item.key))
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid or duplicate mounted row");
                auto row=find(item.key);
                if(!row)row=std::make_shared<Row>(item.value,item.index);
                next.emplace(item.key,row);mounted.push_back({item.key,std::move(row)});
                updates.push_back({item.value,item.index});
            }
            if(auto applied=std::invoke(apply,std::as_const(mounted));!applied)return applied;
            if(!active_)return ii::core::fail(ii::core::ErrorCode::closed,"Collection retired during application");
            rows_=std::move(next);
            std::exception_ptr failure;
            for(std::size_t n=0;n<mounted.size();++n){
                if(!active_)return ii::core::fail(ii::core::ErrorCode::closed,"Collection retired during publication");
                try{mounted[n].row->state.set(std::move(updates[n]));}catch(...){if(!failure)failure=std::current_exception();}
            }
            if(failure)return ii::core::fail(ii::core::ErrorCode::failed,"Collection row publication failed",failure);
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Collection application failed",std::current_exception());}
    }
private:
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true,updating_=false;
    std::map<std::string,std::shared_ptr<Row>,std::less<>> rows_;
};
// Preserve ordinary C++ constness while unwrapping a borrowed shared/raw item.
template<class T> decltype(auto) collection_object(T&& value){
    return member(std::forward<T>(value),[](auto&& object)->decltype(auto){return std::forward<decltype(object)>(object);});
}
template<class Expected,class T> decltype(auto) typed_collection_object(T&& value){
    static_assert(std::same_as<std::remove_cvref_t<T>,Expected>,"Repeated backend item does not match its declared component type");
    return std::forward<T>(value);
}
} // namespace lvui::native
