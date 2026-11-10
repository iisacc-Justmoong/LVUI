#pragma once
#include <LVUI/NativeTabBar.h>
#include <iiGUI/NativeTabBarView.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {
using TabGroupPort=TabGroupPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using TabSectionPort=TabSectionPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using NativeTab=TabBinding<ii::gui::BasicTab,ii::gui::BasicNativeTab,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using NativeTabBar=TabBarBinding<ii::gui::BasicTabBar,ii::gui::BasicNativeTabBar,ii::gui::Widget,ii::gui::WidgetSnapshot>;
}
