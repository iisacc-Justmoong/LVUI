#pragma once
#include <LVUI/NativePageRouter.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <stdexcept>
#include <string>

struct NativeOwnedPageRouterModel final {
    ii::core::Observable<double> width{400.0},height{240.0};
    std::shared_ptr<lvui::PageRouter> router;
    NativeOwnedPageRouterModel(){
        auto home=std::make_shared<lvui::native::FunctionalNavigationPage>([](ii::gui::WidgetBuilder& builder,
                const lvui::NavigationEntry&,const std::shared_ptr<lvui::NavigationContext>&)->ii::core::Result<void>{
            auto label=builder.label({},"home",{.text="Home"});if(!label)return std::unexpected(label.error());return {};
        });
        auto report=std::make_shared<lvui::native::FunctionalNavigationPage>([](ii::gui::WidgetBuilder& builder,
                const lvui::NavigationEntry& entry,const std::shared_ptr<lvui::NavigationContext>&)->ii::core::Result<void>{
            auto id=entry.params.find("id");if(!id)return std::unexpected(id.error());
            auto text=id->get().get<std::string>();if(!text)return std::unexpected(text.error());
            auto label=builder.label({},"report",{.text="Report "+text->get()});if(!label)return std::unexpected(label.error());return {};
        });
        auto created=lvui::PageRouter::create({{"/",home},{"/report/[id]",report}});
        if(!created)throw std::runtime_error(created.error().message);router=std::move(*created);
    }
};
