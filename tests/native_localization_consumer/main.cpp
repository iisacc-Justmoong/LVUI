#include <LVUI/NativeStringCatalog.h>
int main() {
    using namespace lvui::native;
    StringCatalogCodec codec{
        .decode=[](std::string_view text)->ii::core::Result<StringCatalog>{
            return StringCatalog{"ko",{{"title",std::string(text)}}};
        },
        .encode=[](const StringCatalog& catalog)->ii::core::Result<std::string>{
            return catalog.strings.at("title");
        }};
    auto decoded=decode_string_catalog("제목",codec);
    if(!decoded)return 1;
    AppStrings strings;
    if(!strings.replace_catalog(decoded->locale,decoded->strings)||!strings.set_locale("ko-KR"))return 2;
    if(strings.get("title")!="제목")return 3;
    auto encoded=encode_string_catalog(*decoded,codec);
    if(!encoded||!decode_string_catalog(*encoded,codec))return 4;
}
