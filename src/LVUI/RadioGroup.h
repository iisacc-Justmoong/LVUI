#pragma once
#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

namespace lvui {
// A participant changes its local model without callbacks in assign(); publish
// synchronizes the host and may reenter the group. Only published participants
// take part, so an uncommitted candidate cannot change the current selection.
class RadioGroupMember {
public:
    virtual ~RadioGroupMember()=default;
    [[nodiscard]] virtual std::string_view group()const noexcept=0;
    [[nodiscard]] virtual bool selected()const noexcept=0;
    [[nodiscard]] virtual bool participating()const noexcept=0;
    virtual void assign(bool)noexcept=0;
    virtual void publish()noexcept=0;
};

class RadioGroups final {
public:
    // Weak ownership preserves independent generation lifetimes. Registration
    // reserves membership while the candidate is still being built.
    [[nodiscard]] bool prepare(const std::shared_ptr<RadioGroupMember>& member){
        if(!member)return false;
        for(const auto& weak:members_)if(auto current=weak.lock();current==member)return true;
        if(members_.size()>=4096){std::erase_if(members_,[](const auto& weak){return weak.expired();});if(members_.size()>=4096)return false;}
        try{members_.push_back(member);return true;}catch(...){return false;}
    }
    [[nodiscard]] bool select(const std::shared_ptr<RadioGroupMember>& member,bool value)noexcept{
        if(!member||!member->participating()||!contains(member))return false;
        dirty_=true;
        const auto name=member->group();
        if(value&&!name.empty())for(std::size_t i=0;i<members_.size();++i){
            auto other=members_[i].lock();
            if(other&&other!=member&&other->participating()&&other->group()==name&&other->selected())other->assign(false);
        }
        if(member->selected()!=value)member->assign(value);
        return settle();
    }
    // Called after publication and when a selected participant changes group.
    [[nodiscard]] bool refresh(const std::shared_ptr<RadioGroupMember>& member)noexcept{
        if(!member||!member->participating()||!contains(member))return false;
        return select(member,member->selected());
    }
    void retire(const std::shared_ptr<RadioGroupMember>& member)noexcept{
        std::erase_if(members_,[&](const auto& weak){auto current=weak.lock();return !current||current==member;});
        dirty_=true;(void)settle();
    }
private:
    [[nodiscard]] bool contains(const std::shared_ptr<RadioGroupMember>& member)const noexcept{
        for(const auto& weak:members_)if(weak.lock()==member)return true;return false;
    }
    [[nodiscard]] bool settle()noexcept{
        if(flushing_)return true;
        flushing_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{flushing_};
        for(unsigned pass=0;pass<16;++pass){
            dirty_=false;
            for(std::size_t i=0;i<members_.size();++i){
                auto member=members_[i].lock();if(member&&member->participating())member->publish();
                if(dirty_)break;
            }
            if(!dirty_)return true;
        }
        return false;
    }
    std::vector<std::weak_ptr<RadioGroupMember>> members_;
    bool flushing_=false,dirty_=false;
};
} // namespace lvui
