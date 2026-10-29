#pragma once
#include "Text.h"
#include <iiSDKCore/Core/Result/Result.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {
inline constexpr std::size_t maximum_route_bytes=65536,maximum_route_segments=4096;
namespace route_detail {
[[nodiscard]] inline bool whitespace(std::uint32_t c)noexcept {
    return (c>=9&&c<=13)||c==0x20||c==0x85||c==0xa0||c==0x1680||
        (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000;
}
// The caller validates UTF-8 first; decoding only locates scalar boundaries.
[[nodiscard]] inline std::pair<std::uint32_t,std::size_t> scalar(std::string_view value,std::size_t offset)noexcept {
    const auto first=static_cast<unsigned char>(value[offset]);
    if(first<0x80)return {first,1};
    const std::size_t count=first<0xe0?2:first<0xf0?3:4;
    std::uint32_t result=first&(count==2?0x1f:count==3?0x0f:0x07);
    for(std::size_t i=1;i<count;++i)result=(result<<6)|(static_cast<unsigned char>(value[offset+i])&0x3f);
    return {result,count};
}
[[nodiscard]] inline std::vector<std::string_view> segments(std::string_view normalized){
    std::vector<std::string_view> result;if(normalized=="/")return result;
    std::size_t begin=1;
    while(true){const auto end=normalized.find('/',begin);result.push_back(normalized.substr(begin,end==normalized.npos?end:end-begin));
        if(end==normalized.npos)break;begin=end+1;}
    return result;
}
}
[[nodiscard]] inline ii::core::Result<std::string> normalize_route(std::string_view input){
    if(!valid_accessibility_text(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Route must be bounded NUL-free UTF-8");
    std::size_t first=input.size(),last=0;
    for(std::size_t offset=0;offset<input.size();){const auto [value,length]=route_detail::scalar(input,offset);
        if(!route_detail::whitespace(value)){if(first==input.size())first=offset;last=offset+length;}offset+=length;}
    if(first==input.size())return std::string("/");
    auto trimmed=input.substr(first,last-first);std::string result;
    if(trimmed.front()!='/')result.push_back('/');result.append(trimmed);
    while(result.size()>1&&result.back()=='/')result.pop_back();
    if(result.size()>maximum_route_bytes)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Normalized route is too long");
    const auto count=result=="/"?0:1+std::count(result.begin()+1,result.end(),'/');
    if(static_cast<std::size_t>(count)>maximum_route_segments)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route has too many segments");
    return result;
}
using RouteParameters=std::map<std::string,std::string,std::less<>>;
struct RouteMatch final {bool matched=false;RouteParameters params;};

class RoutePattern final {
    enum class Kind {literal,capture,remainder};
    struct Token final {Kind kind;std::string value;};
    bool enabled_=false;
    std::vector<Token> tokens_;
    [[nodiscard]] std::optional<RouteParameters> match_segments(const std::vector<std::string_view>& path)const {
        if(!enabled_)return std::nullopt;RouteParameters captures;std::size_t index=0;
        for(const auto& token:tokens_){
            if(token.kind==Kind::remainder){
                std::string joined;for(std::size_t i=index;i<path.size();++i){if(i!=index)joined.push_back('/');joined.append(path[i]);}
                captures.insert_or_assign(token.value,std::move(joined));return captures;
            }
            if(index==path.size())return std::nullopt;
            if(token.kind==Kind::literal){if(path[index]!=token.value)return std::nullopt;}
            else captures.insert_or_assign(token.value,std::string(path[index]));
            ++index;
        }
        if(index!=path.size())return std::nullopt;return captures;
    }
    friend class RouteResolver;
public:
    [[nodiscard]] static ii::core::Result<RoutePattern> compile(std::string_view pattern){
        RoutePattern result;if(pattern.empty())return result;
        auto normalized=normalize_route(pattern);if(!normalized)return std::unexpected(normalized.error());
        result.enabled_=true;
        for(auto part:route_detail::segments(*normalized)){
            Kind kind=Kind::literal;auto value=part;
            if(part.size()>=2&&part.front()=='['&&part.back()==']'){
                value=part.substr(1,part.size()-2);kind=Kind::capture;
                if(value.starts_with("...")){kind=Kind::remainder;value.remove_prefix(3);}
            }
            result.tokens_.push_back({kind,std::string(value)});
        }
        return result;
    }
    [[nodiscard]] bool enabled()const noexcept{return enabled_;}
    [[nodiscard]] std::size_t token_count()const noexcept{return tokens_.size();}
    [[nodiscard]] ii::core::Result<RouteMatch> match(std::string_view path)const {
        auto normalized=normalize_route(path);if(!normalized)return std::unexpected(normalized.error());
        auto captures=match_segments(route_detail::segments(*normalized));
        return captures?RouteMatch{true,std::move(*captures)}:RouteMatch{};
    }
};

struct RouteResolution final {
    bool matched=false;
    std::size_t index=std::numeric_limits<std::size_t>::max();
    RouteParameters params;
};
struct RouteResolverState final {
    std::uint64_t revision=0,cache_hits=0,cache_misses=0,pattern_matches=0;
    std::size_t patterns=0,cached=0,cache_capacity=256,cache_bytes=0;
};

// Immutable compiled patterns and an owner-thread LRU. Cache entries are
// independently owned, so eviction never invalidates a caller's result.
class RouteResolver final {
    static constexpr std::size_t max_patterns=65536,max_source_bytes=8*1024*1024,max_tokens=262144,max_cache_bytes=8*1024*1024;
    struct Entry final {std::shared_ptr<const RouteResolution> value;std::list<std::string>::iterator position;std::size_t cost;};
    std::thread::id owner_=std::this_thread::get_id();
    std::vector<std::string> source_;
    std::vector<RoutePattern> patterns_;
    std::list<std::string> recent_;
    std::unordered_map<std::string,Entry> cache_;
    RouteResolverState state_;
    [[nodiscard]] ii::core::Result<void> owner()const {
        if(std::this_thread::get_id()!=owner_)return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Route resolver requires its owner thread");return {};
    }
    static void increment(std::uint64_t& value)noexcept {if(value!=std::numeric_limits<std::uint64_t>::max())++value;}
    void evict(){
        while(!recent_.empty()&&(cache_.size()>state_.cache_capacity||state_.cache_bytes>max_cache_bytes)){
            auto found=cache_.find(recent_.back());if(found!=cache_.end()){state_.cache_bytes-=found->second.cost;cache_.erase(found);}recent_.pop_back();
        }
        state_.cached=cache_.size();
    }
public:
    RouteResolver()=default;
    RouteResolver(const RouteResolver&)=delete;RouteResolver& operator=(const RouteResolver&)=delete;
    RouteResolver(RouteResolver&&)=delete;RouteResolver& operator=(RouteResolver&&)=delete;
    [[nodiscard]] ii::core::Result<void> set_patterns(std::vector<std::string> values){
        if(auto checked=owner();!checked)return checked;
        if(values.size()>max_patterns)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Too many route patterns");
        if(values==source_)return {};
        if(state_.revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route revision exhausted");
        std::vector<RoutePattern> prepared;prepared.reserve(values.size());std::size_t bytes=0,tokens=0;
        for(const auto& value:values){
            if(value.size()>max_source_bytes-bytes)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route pattern source budget exceeded");bytes+=value.size();
            auto pattern=RoutePattern::compile(value);if(!pattern)return std::unexpected(pattern.error());
            if(pattern->token_count()>max_tokens-tokens)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route token budget exceeded");
            tokens+=pattern->token_count();prepared.push_back(std::move(*pattern));
        }
        source_=std::move(values);patterns_=std::move(prepared);cache_.clear();recent_.clear();state_.cache_bytes=state_.cached=0;
        state_.patterns=patterns_.size();++state_.revision;return {};
    }
    [[nodiscard]] ii::core::Result<void> set_cache_capacity(std::size_t capacity){
        if(auto checked=owner();!checked)return checked;
        if(capacity>65536)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route cache capacity is too large");
        capacity=std::max<std::size_t>(16,capacity);if(capacity==state_.cache_capacity)return {};
        if(state_.revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Route revision exhausted");
        state_.cache_capacity=capacity;++state_.revision;evict();return {};
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const RouteResolution>> resolve(std::string_view input){
        if(auto checked=owner();!checked)return std::unexpected(checked.error());
        auto path=normalize_route(input);if(!path)return std::unexpected(path.error());
        if(auto found=cache_.find(*path);found!=cache_.end()){
            recent_.splice(recent_.begin(),recent_,found->second.position);increment(state_.cache_hits);return found->second.value;
        }
        increment(state_.cache_misses);const auto pieces=route_detail::segments(*path);auto resolved=std::make_shared<RouteResolution>();
        for(std::size_t index=0;index<patterns_.size();++index){
            if(!patterns_[index].enabled())continue;increment(state_.pattern_matches);
            if(auto params=patterns_[index].match_segments(pieces)){resolved->matched=true;resolved->index=index;resolved->params=std::move(*params);break;}
        }
        std::shared_ptr<const RouteResolution> result=resolved;
        std::size_t cost=sizeof(RouteResolution)+path->size()*2;
        for(const auto& [key,value]:resolved->params)cost+=sizeof(RouteParameters::value_type)+key.size()+value.size();
        if(cost<=max_cache_bytes){
            recent_.push_front(*path);
            try {cache_.emplace(*path,Entry{result,recent_.begin(),cost});}catch(...){recent_.pop_front();throw;}
            state_.cache_bytes+=cost;evict();
        }
        return result;
    }
    [[nodiscard]] ii::core::Result<RouteResolverState> state()const {
        if(auto checked=owner();!checked)return std::unexpected(checked.error());return state_;
    }
};
} // namespace lvui
