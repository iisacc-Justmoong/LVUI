#pragma once
#include <LVUI/NativeMenu.h>
#include <iiGUI/NativeMenuView.h>
#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {
using MenuItemInput=MenuItemInputValue<ii::gui::RasterImage>;
using MenuPopupPort=MenuPopupPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuItemPort=MenuItemPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuSectionPort=MenuSectionPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuSectionAccess=MenuSectionAccessValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuRegistrationPort=MenuRegistrationPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuCapture=MenuCaptureValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using MenuRepeatedRows=MenuRepeatedRowsValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using NativeMenuItem=MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using NativeMenu=MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;
}
