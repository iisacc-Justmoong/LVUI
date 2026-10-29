#pragma once
#include <LVUI/Link.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <vector>

struct NativeOwnedLinkTarget final:lvui::NavigationTarget {
    std::vector<lvui::NavigationRequest> requests;
    ii::core::Result<bool> navigate(const lvui::NavigationRequest& request)override{requests.push_back(request);return true;}
};
struct NativeOwnedLinkPage final:lvui::NavigationPage {};
struct NativeOwnedLinkModel final {
    ii::core::Observable<std::string> text{std::string("Reports")},href{std::string("/reports")};
    ii::core::Observable<ii::core::Value> params{ii::core::Value(ii::core::Value::Object{{"id",ii::core::Value(42)}})};
    ii::core::Observable<bool> enabled{true},replace{false},underline{true},show_nested{true};
    ii::core::Observable<ii::gui::WidgetColor> color{ii::gui::WidgetColor{.2,.4,.6,1}};
    ii::core::Observable<std::shared_ptr<lvui::NavigationTarget>> router{std::shared_ptr<lvui::NavigationTarget>{}};
    ii::core::Observable<std::shared_ptr<lvui::NavigationPage>> page{std::shared_ptr<lvui::NavigationPage>{}};
    std::shared_ptr<NativeOwnedLinkTarget> navigator=std::make_shared<NativeOwnedLinkTarget>();
    std::vector<std::string> events;
    void log(const lvui::LinkEvent& event){events.push_back("method:"+event.request.href);}
    void after(){events.push_back("after");}
    void clicked(){events.push_back("clicked");}
    void navigated(bool value){events.push_back(value?"navigated:true":"navigated:false");}
};
