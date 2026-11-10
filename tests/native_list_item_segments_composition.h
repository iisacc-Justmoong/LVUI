#pragma once
#include <LVUI/NativeListItemSegments.h>
#include <iiGUI/NativeListItemSegmentsView.h>
#include <iiGUI/SvgImageCache.h>
namespace lvui::native {
using SegmentButtonEvent=ButtonEventValue<ii::gui::Widget>;
using SegmentButtonMethod=ButtonMethodValue<SegmentButtonEvent>;
using SegmentButtonRegistry=ButtonMethodRegistryValue<SegmentButtonEvent>;
using SegmentButton=ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
using SegmentGroup=SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,
    SegmentButtonMethod,SegmentButtonRegistry,ButtonMethodInvoker>;
using NativeListItemSegments=ListItemSegmentsBinding<ii::gui::BasicListItemSegments,ii::gui::BasicNativeListItemSegments,
    SegmentButton,SegmentGroup,ii::gui::RasterImage>;
}
