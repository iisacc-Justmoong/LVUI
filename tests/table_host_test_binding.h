#pragma once
#include <LVUI/NativeTable.h>
#include <iiGUI/NativeTableView.h>
#include <iiGUI/TableViewport.h>
#include <iiGUI/NativeInputFieldView.h>
#include <iiGUI/NativeMenuView.h>
#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/TableHeaderPresentation.h>
#include <iiGUI/TableViewGroup.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {
using TableTestField=InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;
using TableTestMenu=MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using TableTestMenuItem=MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;
using TableTestHeader=TableHeaderPresentationBinding<ii::gui::BasicTableHeaderPresentation,ii::gui::CachedSvgDecoder>;
using TableTestGroup=TableViewGroupBinding<ii::gui::BasicNativeTableViewGroup,ii::gui::WidgetBuilder,ii::gui::Rect>;
using NativeTableViews=NativeTableViewsValue<ii::gui::WidgetBuilder>;
using NativeTable=TableBinding<ii::gui::BasicNativeTable,TableTestField,TableTestMenu,TableTestMenuItem,TableTestHeader,TableTestGroup,ii::gui::WidgetBuilder,ii::gui::Rect,ii::gui::RasterImage,lvui::TableViewportBinding<ii::gui::BasicTableViewportSource>>;
}
