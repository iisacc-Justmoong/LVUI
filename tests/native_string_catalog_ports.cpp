#include <LVUI/NativeStringCatalog.h>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
void good(ii::core::Result<void> result) { if(!result) throw std::runtime_error(result.error().message); }
}
int main() {
    using namespace lvui::native;
    using ii::core::ErrorCode;
    std::map<std::filesystem::path,std::string> files{{"catalog", "ko|greeting|안녕"}};
    int reads=0,decodes=0,writes=0;
    StringCatalogStorage storage{
        .read=[&](const auto& path,std::size_t limit)->ii::core::Result<std::string>{
            ++reads;auto found=files.find(path);
            if(found==files.end())return ii::core::fail(ErrorCode::not_found,"missing");
            if(found->second.size()>limit)return ii::core::fail(ErrorCode::full,"large");
            return found->second;
        },
        .write=[&](const auto& path,std::string_view bytes)->ii::core::Result<void>{
            ++writes;files[path]=bytes;return {};
        }};
    StringCatalogCodec codec{
        .decode=[&](std::string_view bytes)->ii::core::Result<StringCatalog>{
            ++decodes;if(bytes=="invalid")return ii::core::fail(ErrorCode::invalid_argument,"invalid");
            if(bytes=="bad-locale")return StringCatalog{"ko--KR",{{"greeting","bad"}}};
            if(bytes=="bad-text")return StringCatalog{"ko",{{"greeting",std::string(1,'\0')}}};
            return StringCatalog{"ko",{{"greeting",std::string(bytes.substr(bytes.rfind('|')+1))}}};
        },
        .encode=[](const StringCatalog& catalog)->ii::core::Result<std::string>{
            return catalog.locale+"|greeting|"+catalog.strings.at("greeting");
        }};
    AppStrings strings;
    good(load_string_catalog(strings,"catalog",storage,codec));
    good(strings.set_locale("ko-KR"));
    require(strings.get("greeting")=="안녕", "Storage/codec result was not applied");
    files["catalog"]="invalid";
    require(!load_string_catalog(strings,"catalog",storage,codec), "Invalid data was accepted");
    require(strings.get("greeting")=="안녕", "Failed decode changed live strings");
    for(const auto* input:{"bad-locale","bad-text"}){
        files["catalog"]=input;
        require(!load_string_catalog(strings,"catalog",storage,codec), "Invalid codec output was accepted");
        require(strings.get("greeting")=="안녕", "Invalid codec output changed live strings");
    }
    const int before=decodes;
    require(!decode_string_catalog(std::string(AppStrings::maximum_catalog_bytes+1,' '),codec) &&
            decodes==before,"Oversized input reached the codec");
    require(!load_string_catalog(strings,"missing",storage,codec), "Missing resource was accepted");
    good(strings.set_string("greeting","changed"));
    good(save_string_catalog(strings,"ko-kr","saved",storage,codec));
    require(writes==1 && files.at("saved")=="ko-kr|greeting|changed", "Storage did not receive encoded output");
    require(!load_string_catalog(strings,"catalog",StringCatalogStorage{},codec), "Missing reader was accepted");
    require(!decode_string_catalog("input",StringCatalogCodec{}), "Missing decoder was accepted");
    auto throwing=codec;throwing.decode=[](std::string_view)->ii::core::Result<StringCatalog>{throw std::runtime_error("bad");};
    require(!decode_string_catalog("input",throwing), "Exception escaped the port boundary");
    require(reads>=5,"Expected storage reads were not made");
}
