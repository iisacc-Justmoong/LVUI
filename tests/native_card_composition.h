#pragma once
#include <LVUI/NativeCard.h>
#include <iiGUI/NativeCardView.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {using CardInput=CardInputValue<ii::gui::RasterImage>;using NativeCardViews=NativeCardViewsValue<ii::gui::WidgetFactory>;
using NativeCard=CardBinding<ii::gui::BasicCard,ii::gui::BasicNativeCard,ii::gui::CachedSvgDecoder,ii::gui::WidgetFactory>;}
