#pragma once
#include <iiSDKCore/Core/Result/Result.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace lvui::native {
template<class Port>
using SectionGroup = typename std::remove_cvref_t<decltype(std::declval<Port>().layout_groups)>::value_type;
// Transactional, ordered and keyed content publication for reusable controls.
// The supplied Port must carry layout_groups and is otherwise control-specific.
template<class Port>
class SectionCapture final : public std::enable_shared_from_this<SectionCapture<Port>> {
    using SectionPort=std::function<ii::core::Result<void>(std::vector<Port>)>;
    using Ports=std::vector<Port>;
    struct Slot final {std::vector<SectionGroup<Port>> groups;Ports items;};
    using Segment=std::variant<Port,Slot>;
public:
    SectionCapture()=default;
    SectionCapture(const SectionCapture&)=delete;
    SectionCapture& operator=(const SectionCapture&)=delete;
    [[nodiscard]] ii::core::Result<void> add_group(SectionGroup<Port> group){
        if(auto valid=staging();!valid)return valid;
        if(groups_.size()>=64)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
        try{groups_.push_back(std::move(group));return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section group capture failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> append(Port item){
        if(auto valid=staging();!valid)return valid;
        if(segments_.size()>=4096||item_count_>=4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section item capture limit exceeded");
        if(item.layout_groups.size()>64-groups_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
        try{item.layout_groups.insert(item.layout_groups.end(),groups_.begin(),groups_.end());segments_.emplace_back(std::move(item));++item_count_;return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section item capture failed",std::current_exception());}
    }
    // A slot retains its declaration position while its contents change.
    // Staged updates stay private until the complete generation is published.
    [[nodiscard]] ii::core::Result<SectionPort> slot(){
        if(auto valid=staging();!valid)return std::unexpected(valid.error());
        if(segments_.size()>=4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section capture slot limit exceeded");
        try{
            auto weak=this->weak_from_this();if(weak.expired())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Section capture slots require shared ownership");
            const auto index=segments_.size();
            SectionPort port{[weak,index](Ports values)->ii::core::Result<void>{auto owner=weak.lock();
                if(!owner)return ii::core::fail(ii::core::ErrorCode::closed,"Section capture retired");return owner->replace(index,std::move(values));}};
            segments_.emplace_back(Slot{groups_,{}});return port;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section capture slot creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> publish(SectionPort section){
        if(auto valid=staging();!valid)return valid;
        if(!section)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Section capture requires a section port");
        try{
            auto candidate=flatten();if(!candidate)return std::unexpected(candidate.error());
            const auto generation=generation_;updating_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{updating_};
            auto applied=section(std::move(*candidate));if(!applied)return applied;
            if(closed_||generation_!=generation)return ii::core::fail(ii::core::ErrorCode::cancelled,"Section capture retired during publication");
            section_=std::move(section);published_=true;return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section capture publication failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::vector<Port>> release(){
        if(auto valid=staging();!valid)return std::unexpected(valid.error());
        try{auto result=flatten();if(!result)return std::unexpected(result.error());closed_=true;++generation_;segments_.clear();groups_.clear();item_count_=0;return result;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section capture release failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> close(){
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Section capture requires its owner thread");
        if(closed_)return {};closed_=true;++generation_;section_={};segments_.clear();groups_.clear();item_count_=0;return {};
    }
private:
    [[nodiscard]] ii::core::Result<void> check()const{
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Section capture requires its owner thread");
        if(closed_)return ii::core::fail(ii::core::ErrorCode::closed,"Section capture has retired");return {};
    }
    [[nodiscard]] ii::core::Result<void> staging()const{
        if(auto valid=check();!valid)return valid;
        if(updating_||published_)return ii::core::fail(ii::core::ErrorCode::busy,"Section capture is not staging");return {};
    }
    [[nodiscard]] ii::core::Result<Ports> flatten(std::size_t changed=(std::numeric_limits<std::size_t>::max)(),const Ports* replacement=nullptr)const{
        Ports result;
        for(std::size_t i=0;i<segments_.size();++i){
            if(const auto item=std::get_if<Port>(&segments_[i])){
                if(result.size()==4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section item capture limit exceeded");result.push_back(*item);
            }else{
                const auto& items=i==changed?*replacement:std::get<Slot>(segments_[i]).items;
                if(items.size()>4096-result.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section item capture limit exceeded");
                result.insert(result.end(),items.begin(),items.end());
            }
        }
        return result;
    }
    [[nodiscard]] ii::core::Result<void> replace(std::size_t index,Ports candidate){
        if(auto valid=check();!valid)return valid;
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Section capture is publishing");
        if(index>=segments_.size()||!std::holds_alternative<Slot>(segments_[index]))return ii::core::fail(ii::core::ErrorCode::closed,"Section capture slot retired");
        const auto previous=std::get<Slot>(segments_[index]).items.size();
        if(candidate.size()>4096-(item_count_-previous))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section item capture limit exceeded");
        try{
            const auto& groups=std::get<Slot>(segments_[index]).groups;
            for(auto& item:candidate){
                if(item.layout_groups.size()>64-groups.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
                item.layout_groups.insert(item.layout_groups.end(),groups.begin(),groups.end());
            }
            auto combined=flatten(index,&candidate);if(!combined)return std::unexpected(combined.error());
            if(!published_){item_count_=item_count_-previous+candidate.size();std::get<Slot>(segments_[index]).items=std::move(candidate);return {};}
            auto section=section_;const auto generation=generation_;updating_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{updating_};
            if(!section)return ii::core::fail(ii::core::ErrorCode::closed,"Section capture section retired");
            if(auto applied=section(std::move(*combined));!applied)return applied;
            if(closed_||generation_!=generation)return ii::core::fail(ii::core::ErrorCode::cancelled,"Section capture retired during publication");
            item_count_=item_count_-previous+candidate.size();std::get<Slot>(segments_[index]).items=std::move(candidate);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Section capture slot publication failed",std::current_exception());}
    }
    const std::thread::id owner_=std::this_thread::get_id();
    bool closed_=false,published_=false,updating_=false;
    std::uint64_t generation_=0;
    std::size_t item_count_=0;
    SectionPort section_;
    std::vector<SectionGroup<Port>> groups_;
    std::vector<Segment> segments_;
};
template<class Port>
class SectionRepeatedRows final : public std::enable_shared_from_this<SectionRepeatedRows<Port>> {
public:
    using SectionPort=std::function<ii::core::Result<void>(std::vector<Port>)>;
    using Ports=std::vector<Port>;
    class Row final {
        friend class SectionRepeatedRows;
        std::weak_ptr<SectionRepeatedRows> owner_;
        std::string key_;
        Ports ports_;
        std::vector<SectionGroup<Port>> groups_;
        bool active_=false,retired_=false;
        Row(std::weak_ptr<SectionRepeatedRows> owner,std::string key):owner_(std::move(owner)),key_(std::move(key)){}
    public:
        [[nodiscard]] ii::core::Result<void> add_group(SectionGroup<Port> group){
            auto owner=owner_.lock();if(!owner)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section owner retired");
            if(auto valid=owner->check();!valid)return valid;
            if(active_||retired_||owner->updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section row is not staging");
            if(groups_.size()>=64)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
            try{groups_.push_back(std::move(group));return {};}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section group capture failed",std::current_exception());}
        }
        [[nodiscard]] ii::core::Result<void> append(Port item){
            auto owner=owner_.lock();if(!owner)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section owner retired");
            if(auto valid=owner->check();!valid)return valid;
            if(active_||retired_||owner->updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section row is not staging");
            if(ports_.size()>=4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Repeated section item limit exceeded");
            if(item.layout_groups.size()>64-groups_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
            try{item.layout_groups.insert(item.layout_groups.end(),groups_.begin(),groups_.end());ports_.push_back(std::move(item));return {};}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section row registration failed",std::current_exception());}
        }
        [[nodiscard]] ii::core::Result<void> replace(Ports items){
            auto owner=owner_.lock();if(!owner)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section owner retired");
            return owner->replace(*this,std::move(items));
        }
    };
    struct Mount final {std::string key;std::shared_ptr<Row> row;};
    explicit SectionRepeatedRows(SectionPort section):section_(std::move(section)){}
    SectionRepeatedRows(const SectionRepeatedRows&)=delete;
    SectionRepeatedRows& operator=(const SectionRepeatedRows&)=delete;
    [[nodiscard]] ii::core::Result<std::shared_ptr<Row>> row(std::string key){
        if(auto valid=check();!valid)return std::unexpected(valid.error());
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section is publishing");
        try{if(auto found=rows_.find(key);found!=rows_.end())return found->second;
            if(key.empty()||key.size()>1024)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Repeated section row key must contain 1..1024 bytes");
            auto owner=this->weak_from_this();if(owner.expired())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Repeated section requires shared ownership");
            return std::shared_ptr<Row>(new Row(std::move(owner),std::move(key)));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section row staging failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> publish(std::vector<Mount> mounts){
        if(auto valid=check();!valid)return valid;
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section is publishing");
        if(mounts.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Repeated section row limit exceeded");
        updating_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{updating_};
        try{
            std::map<std::string,std::shared_ptr<Row>,std::less<>> next;
            std::vector<std::string> order;Ports ports;order.reserve(mounts.size());
            for(const auto& mount:mounts){
                if(!mount.row||mount.key!=mount.row->key_||mount.row->retired_||mount.row->owner_.lock().get()!=this||!next.emplace(mount.key,mount.row).second)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid repeated section row publication");
                if(auto old=rows_.find(mount.key);old!=rows_.end()&&old->second!=mount.row)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Repeated section row identity changed");
                if(mount.row->ports_.size()>4096-ports.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Repeated section item limit exceeded");
                order.push_back(mount.key);ports.insert(ports.end(),mount.row->ports_.begin(),mount.row->ports_.end());
            }
            if(!section_)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section section retired");
            if(auto applied=section_(std::move(ports));!applied)return applied;
            for(auto& [key,row]:rows_)if(!next.contains(key)){row->active_=false;row->retired_=true;}
            for(auto& [key,row]:next)row->active_=true;
            rows_=std::move(next);order_=std::move(order);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section publication failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> close(){
        if(auto valid=check();!valid)return valid;
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section is publishing");
        active_=false;for(auto& [key,row]:rows_){row->active_=false;row->retired_=true;}rows_.clear();order_.clear();section_={};return {};
    }
private:
    [[nodiscard]] ii::core::Result<void> check()const{
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Repeated section requires its owner thread");
        if(!active_)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section is closed");return {};
    }
    [[nodiscard]] ii::core::Result<void> replace(Row& row,Ports candidate){
        if(auto valid=check();!valid)return valid;
        if(updating_)return ii::core::fail(ii::core::ErrorCode::busy,"Repeated section is publishing");
        if(row.retired_)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section row retired");
        if(candidate.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Repeated section item limit exceeded");
        try{for(auto& item:candidate){
            if(item.layout_groups.size()>64-row.groups_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Section group nesting limit exceeded");
            item.layout_groups.insert(item.layout_groups.end(),row.groups_.begin(),row.groups_.end());
        }}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section group capture failed",std::current_exception());}
        if(!row.active_){row.ports_=std::move(candidate);return {};}
        const auto found=rows_.find(row.key_);
        if(found==rows_.end()||found->second.get()!=&row)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section row retired");
        updating_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{updating_};
        try{
            Ports ports;for(const auto& key:order_){const auto& item=*rows_.at(key);const auto& values=&item==&row?candidate:item.ports_;
                if(values.size()>4096-ports.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Repeated section item limit exceeded");
                ports.insert(ports.end(),values.begin(),values.end());}
            if(!section_)return ii::core::fail(ii::core::ErrorCode::closed,"Repeated section section retired");
            if(auto applied=section_(std::move(ports));!applied)return applied;
            row.ports_=std::move(candidate);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Repeated section row replacement failed",std::current_exception());}
    }
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true,updating_=false;
    SectionPort section_;
    std::map<std::string,std::shared_ptr<Row>,std::less<>> rows_;
    std::vector<std::string> order_;
};
} // namespace lvui::native
