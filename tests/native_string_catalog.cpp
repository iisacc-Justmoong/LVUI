#include <LVUI/NativeStringCatalog.h>
#include "../example/GettingStarted/App/StringCatalogAdapters.h"
#include <iiFileProviderNative.h>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
}
int main(int argc,char** argv){
    if(argc!=3)throw std::runtime_error("Expected example resources and a temporary resource workspace");
    using namespace lvui::native;
    const std::filesystem::path source=argv[1], workspace=argv[2];
    const auto storage=lvui_example::catalog_storage();
    const auto codec=lvui_example::catalog_codec();
    AppStrings strings;
    good(load_string_catalog(strings,source/"en.json",storage,codec));
    good(load_string_catalog(strings,source/"ko.json",storage,codec));
    check(strings.get("counter.heading")=="Current count","External English catalog did not load");
    good(strings.set_locale("ko-KR"));
    check(strings.get("counter.heading")=="현재 숫자","External Korean catalog or locale fallback failed");
    Bindings graph;std::string displayed;
    check(bool(graph.bind([&](ReadContext& context){return strings.read(context,"counter.heading");},
        [&](const std::string& value)->ii::core::Result<void>{displayed=value;return {}; })),"External string binding failed");
    const auto edited=workspace/"edited-ko.json";
    iiFileProvider::native::File::write(edited,R"({"locale":"ko","strings":{"counter.heading":"파일에서 수정"}})");
    good(load_string_catalog(strings,edited,storage,codec));
    check(displayed=="파일에서 수정","Reloading an externally edited file did not update active bindings");
    good(strings.set_string("counter.heading","런타임 변경"));
    const auto saved=workspace/"ko-kr.json";
    good(save_string_catalog(strings,"ko-kr",saved,storage,codec));
    AppStrings restored;
    good(load_string_catalog(restored,saved,storage,codec));good(restored.set_locale("ko-kr"));
    check(restored.get("counter.heading")=="런타임 변경","Saved runtime replacement did not round trip");
    const auto prior=restored.get("counter.heading");
    for(const auto& invalid:{"{\"locale\":\"ko\",\"strings\":{\"a\":\"1\",\"a\":\"2\"}}",
                             "{\"locale\":\"ko\",\"strings\":{\"a\":7}}",
                             "{\"locale\":\"ko\",\"strings\":{},\"extra\":true}",
                             "{\"locale\":\"ko--KR\",\"strings\":{}}",
                             "{\"locale\":\"ko\",\"strings\":{\"a\":\"\\u0000\"}}"}){
        iiFileProvider::native::File::write(saved,std::string(invalid));
        check(!load_string_catalog(restored,saved,storage,codec)&&restored.get("counter.heading")==prior,
              "Invalid external catalog partially replaced live strings");
    }
    check(!decode_string_catalog(std::string(AppStrings::maximum_catalog_bytes+1,' '),codec),"Oversized JSON catalog was accepted");
    const auto encoded=take(encode_string_catalog({"en",{{"unicode","한글 🌍\n\"text\""},{"empty",""}}},codec));
    const auto decoded=take(decode_string_catalog(encoded,codec));
    check(decoded.locale=="en"&&decoded.strings.at("unicode")=="한글 🌍\n\"text\""&&decoded.strings.at("empty").empty(),
          "UTF-8 catalog did not round trip");
    good(graph.close());
}
