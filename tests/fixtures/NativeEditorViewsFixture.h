#pragma once
#include <iiGUI/NativeEditorCanvasView.h>
#include <LVUI/NativeEditorTimeline.h>
#include <iiGUI/NativeEditorTimelineView.h>
#include <iiGUI/NativeView.h>
#include <iiSDKCore/Core/Property/Observable.h>

class NativeEditorCanvasSource final:public ii::gui::NativeViewSource {
public:
    mutable std::optional<ii::gui::NativeEditorCanvas> view;
    mutable int queries=0,events=0;
    ii::core::Result<void> build(ii::gui::WidgetBuilder& b)const override{
        using namespace ii::gui;NativeEditorCanvasCallbacks callbacks;
        callbacks.query=[this](const EditorCanvasQuery&)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
            ++queries;return std::shared_ptr<const ii::gui::WidgetDrawing>(std::make_shared<ii::gui::WidgetDrawing>());};
        callbacks.input=[this](const EditorCanvasInput&)->ii::core::Result<void>{++events;return {};};
        auto value=NativeEditorCanvas::create(b,{},"editor-canvas",{.width=400,.height=100},callbacks);
        if(!value)return std::unexpected(value.error());view=*value;return {};
    }
};
class NativeEditorTimelineSource final:public ii::gui::NativeViewSource {
public:
    mutable std::optional<ii::gui::NativeEditorTimeline> view;
    mutable int queries=0,events=0;
    ii::core::Result<void> build(ii::gui::WidgetBuilder& b)const override{
        using namespace ii::gui;NativeEditorTimelineCallbacks callbacks;
        callbacks.query=[this](const EditorTimelineQuery&)->ii::core::Result<EditorTimelineProjection>{++queries;
            return EditorTimelineProjection{.tracks={{"track","Images",0,40}},.clips={{"clip","track","Image",0,20}}};};
        callbacks.input=[this](const EditorTimelineEvent&)->ii::core::Result<void>{++events;return {};};
        EditorTimelineOptions options{.width=400,.height=160,.frame_count=100,.pixels_per_frame=2};
        options.palette=lvui::native::editor_timeline_palette<EditorTimelinePalette>();
        auto value=NativeEditorTimeline::create(b,{},"editor-timeline",options,callbacks);
        if(!value)return std::unexpected(value.error());view=*value;return {};
    }
};
struct NativeEditorViewsModel final {
    std::shared_ptr<NativeEditorCanvasSource> canvas_source=std::make_shared<NativeEditorCanvasSource>();
    std::shared_ptr<NativeEditorTimelineSource> timeline_source=std::make_shared<NativeEditorTimelineSource>();
    ii::core::Observable<std::shared_ptr<ii::gui::NativeViewSource>> canvas{canvas_source},timeline{timeline_source};
};
