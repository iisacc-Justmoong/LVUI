#pragma once
#include <LVUI/NativePageRouter.h>
#include <iiGUI/NativePageRouterView.h>
namespace lvui::native {
using NativeNavigationPage=NativeNavigationPageValue<ii::gui::WidgetBuilder>;
using NativePageRouter=PageRouterBinding<ii::gui::BasicNativePageRouter,ii::gui::WidgetBuilder>;
}
