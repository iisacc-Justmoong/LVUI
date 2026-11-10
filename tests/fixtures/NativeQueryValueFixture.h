#pragma once

#include <iiSDKCore/Runtime/QueryValue.h>
#include <cstdint>
#include <string>

struct NativeQueryValueRead final {};
struct NativeQueryValueWrite final { std::string text; };

struct NativeQueryValueModel final {
    std::string stored="Draft";
    int reads=0,writes=0;
    ii::core::Observable<std::uint64_t> revision{0};
    ii::runtime::QueryPort<NativeQueryValueRead,std::string> read{[this](const auto&){++reads;return stored;}};
    ii::runtime::QueryPort<NativeQueryValueWrite,ii::core::Result<void>> write{[this](const auto& request)->ii::core::Result<void>{
        ++writes;
        if(request.text.size()>8)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Name is too long");
        if(stored!=request.text){stored=request.text;revision.set(revision.get()+1);}
        return {};
    }};
    using Value=ii::runtime::QueryValue<std::string,NativeQueryValueRead,NativeQueryValueWrite>;
    Value value{read,{},write,[](std::string text){return NativeQueryValueWrite{std::move(text)};},revision};
    const Value readonly{read,{},write,[](std::string text){return NativeQueryValueWrite{std::move(text)};},revision};
    std::string current() const { return value.get(); }
};
