#include <LVUI/NativeAppStrings.h>
#include <LVUI/NativeBackendFactories.h>
#include <LVUI/NativeBindings.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
}
}

int main() {
    using namespace lvui::native;
    auto strings = std::make_shared<AppStrings>();
    good(strings->replace_catalog("en", {{"title", "Title"}, {"body", "Body"}, {"blank", "fallback"}}));
    good(strings->replace_catalog("ko", {{"title", "제목"}, {"blank", ""}}));
    good(strings->replace_catalog("ko-KR", {{"body", "본문"}}));
    auto factories = BackendFactories{}.with_app_strings(strings).with<int>([]{return 1;})
        .with_activation([]{return BackendActivation{};});
    check(factories.app_strings() == strings, "Factory composition lost app string storage");
    Bindings first, second; std::string title, body; int title_changes=0;bool reentered=false;
    check(bool(first.bind([&](ReadContext& context){return strings->read(context,"title");},
        [&](const std::string& value)->ii::core::Result<void>{title=value;++title_changes;
            if(value=="trigger"&&!reentered){reentered=true;good(strings->set_string("body","reentered"));}return {}; })), "Title binding failed");
    check(bool(second.bind([&](ReadContext& context){return strings->read(context,"body");},
        [&](const std::string& value)->ii::core::Result<void>{body=value;return {}; })), "Body binding failed");
    check(title=="Title"&&body=="Body"&&strings->get("missing")=="missing", "Default or missing strings are incorrect");
    good(strings->set_locale("KO-kr"));
    check(title=="제목"&&body=="본문"&&strings->get("blank").empty(), "Locale hierarchy or empty translation failed");
    good(strings->set_string("title","다른 제목"));
    check(title=="다른 제목"&&body=="본문", "Runtime replacement did not update live bindings");
    const auto before=title_changes;
    good(strings->set_string("body","새 본문"));
    check(body=="새 본문"&&title_changes==before, "An unrelated translation reapplied an unchanged value");
    auto independent=std::make_shared<AppStrings>();
    good(independent->replace_catalog("en",{{"title","Independent"}}));
    check(independent->get("title")=="Independent"&&strings->get("title")=="다른 제목", "App stores leaked between hosts");
    check(!strings->set_locale("ko--KR")&&!strings->set_string(std::string("bad\0key",7),"x"), "Invalid locale or key was accepted");
    check(!strings->replace_catalog("ko",{{"title",std::string("\xff",1)}})&&strings->get("title")=="다른 제목", "Invalid UTF-8 replaced valid strings");
    check(!strings->set_string("too.large",std::string(AppStrings::maximum_value_bytes+1,'x')),
          "Oversized runtime string was accepted");
    bool wrong=false;std::thread worker([&]{auto result=strings->set_locale("en");wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    check(wrong&&strings->get("title")=="다른 제목", "A foreign thread changed the UI-owned store");
    good(strings->replace_catalog("ja",{{"fallback.only","日本語"}}));good(strings->set_fallback_locale("ja-JP"));
    check(strings->get("fallback.only")=="日本語","Fallback locale hierarchy was not searched");
    good(strings->set_string("title","trigger"));
    check(reentered&&body=="reentered"&&title=="trigger","Reentrant string updates lost a committed value");
    good(first.close());good(second.close());
}
