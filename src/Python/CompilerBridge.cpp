#include "CompilerBridge.h"
#include <LVUI/Compiler.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <new>
#include <string>
#include <string_view>
#include <utility>

struct lvui_compiler_result final {
    int status=LVUI_COMPILER_OK,stage=LVUI_COMPILER_SOURCE;
    std::size_t offset=0,line=0,column=0;
    std::string data;
};

namespace {
constexpr std::size_t source_limit=1024*1024,argument_limit=4096,include_limit=256;
bool valid(lvui_compiler_bytes value,std::size_t limit,bool allow_nul=false)noexcept{
    if(value.size>limit||(!value.data&&value.size))return false;
    const std::string_view text=value.size?std::string_view(value.data,value.size):std::string_view{};
    return ii::core::valid_utf8(text)&&(allow_nul||text.find('\0')==text.npos);
}
std::string_view view(lvui_compiler_bytes value)noexcept{
    return value.size?std::string_view(value.data,value.size):std::string_view{};
}
void invalid(lvui_compiler_result& result,std::string_view message){
    result.status=LVUI_COMPILER_INVALID_ARGUMENT;result.data=message;
}
void diagnostic(lvui_compiler_result& result,const lvui::Diagnostic& error,int stage){
    result.status=LVUI_COMPILER_DIAGNOSTIC;result.stage=stage;result.offset=error.offset;
    result.line=error.line;result.column=error.column;result.data=error.message;
}
template<class Work>lvui_compiler_result* execute(Work&& work)noexcept{
    auto* result=new(std::nothrow) lvui_compiler_result;
    if(!result)return nullptr;
    try{work(*result);}
    catch(...){result->status=LVUI_COMPILER_INTERNAL_ERROR;try{result->data="Native compiler bridge failed";}catch(...){}}
    return result;
}
}

extern "C" {
int lvui_compiler_abi_version(void)noexcept{return 1;}
lvui_compiler_result* lvui_compiler_compile(lvui_compiler_bytes source,lvui_compiler_bytes name,
        const lvui_compiler_bytes* includes,std::size_t include_count,const lvui_compiler_bytes* style_profile)noexcept{
    return execute([&](auto& result){
        if(!valid(source,source_limit,true)||!valid(name,argument_limit)||include_count>include_limit||
           (include_count&&!includes)||(style_profile&&!valid(*style_profile,source_limit,true))){
            invalid(result,"Invalid LVUI source or compiler argument");return;
        }
        lvui::NativeCompileOptions options;options.name=view(name);
        for(std::size_t i=0;i<include_count;++i){
            if(!valid(includes[i],argument_limit)){invalid(result,"Invalid LVUI include argument");return;}
            options.includes.emplace_back(view(includes[i]));
        }
        if(style_profile){
            auto style=lvui::compile_style_profile(view(*style_profile));
            if(!style){diagnostic(result,style.error(),LVUI_COMPILER_STYLE);return;}
            options.style_tokens=std::move(*style);
        }
        auto compiled=lvui::compile_native(view(source),options);
        if(!compiled){diagnostic(result,compiled.error(),LVUI_COMPILER_SOURCE);return;}
        result.data=std::move(*compiled);
    });
}
lvui_compiler_result* lvui_compiler_styles(lvui_compiler_bytes profile,lvui_compiler_bytes cpp_namespace)noexcept{
    return execute([&](auto& result){
        if(!valid(profile,source_limit,true)||!valid(cpp_namespace,argument_limit)){
            invalid(result,"Invalid LVUI style profile or namespace");return;
        }
        auto style=lvui::compile_style_profile(view(profile));
        if(!style){diagnostic(result,style.error(),LVUI_COMPILER_STYLE);return;}
        auto header=lvui::compile_style_header(*style,view(cpp_namespace));
        if(!header){diagnostic(result,header.error(),LVUI_COMPILER_HEADER);return;}
        result.data=std::move(*header);
    });
}
int lvui_compiler_result_status(const lvui_compiler_result* result)noexcept{return result?result->status:LVUI_COMPILER_INTERNAL_ERROR;}
int lvui_compiler_result_stage(const lvui_compiler_result* result)noexcept{return result?result->stage:LVUI_COMPILER_SOURCE;}
std::size_t lvui_compiler_result_offset(const lvui_compiler_result* result)noexcept{return result?result->offset:0;}
std::size_t lvui_compiler_result_line(const lvui_compiler_result* result)noexcept{return result?result->line:0;}
std::size_t lvui_compiler_result_column(const lvui_compiler_result* result)noexcept{return result?result->column:0;}
const char* lvui_compiler_result_data(const lvui_compiler_result* result)noexcept{return result?result->data.data():"";}
std::size_t lvui_compiler_result_size(const lvui_compiler_result* result)noexcept{return result?result->data.size():0;}
void lvui_compiler_result_free(lvui_compiler_result* result)noexcept{delete result;}
}
