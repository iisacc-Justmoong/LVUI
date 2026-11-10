#pragma once
#include <LVUI/NativeListItemSelector.h>
#include <iiGUI/NativeListItemSelectorView.h>
#include <iiGUI/NativeComboBoxView.h>
#include <iiGUI/NativeMenuView.h>
#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {
using NativeComboBox=ComboBoxBinding<ii::gui::BasicComboBox,ii::gui::BasicNativeComboBox,ii::gui::Widget>;
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
using ListItemSelector=ii::gui::BasicListItemSelector<ListItemSelectorModelPolicy>;
using NativeListItemSelector=ListItemSelectorBinding<ii::gui::BasicListItemSelector,ii::gui::BasicNativeListItemSelector,
    NativeComboBox,NativeMenu,NativeMenuItem,ii::gui::RasterImage>;
}
