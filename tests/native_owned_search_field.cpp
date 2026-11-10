#include <LVUI/NativeValues.h>
#include <LVUI/NativeSearchField.h>
#include <iiGUI/NativeSearchFieldView.h>
#include <iiGUI/SvgImageCache.h>
using SearchFieldTestInput=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;
using SearchFieldTestMaterial=ii::gui::BasicTextFieldMaterial<lvui::native::text_field_material::Policy>;
using SearchFieldTestView=lvui::native::SearchFieldBinding<ii::gui::BasicNativeSearchField,SearchFieldTestInput,SearchFieldTestMaterial,ii::gui::CachedSvgDecoder,ii::gui::NativeTextAffinity>;
#include <iiGUI/WidgetState.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    for(const auto& item:take(tree->snapshots()))if(item.id==suffix||item.id.ends_with(std::string("/")+std::string(suffix)))
        return take(tree->find(item.id));
    throw std::runtime_error("Search widget was not mounted: "+std::string(suffix));
}

void search_field(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));
    std::optional<SearchFieldTestView> field;
    std::vector<std::string> edits;
    lvui::SearchFieldConfiguration options;options.width=220;options.placeholder="Find a size";
    options.accessibility_name="Search sizes";options.clear_accessibility_name="Clear search";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto parent=builder.absolute_stack({},"parent",{400,100});if(!parent)return std::unexpected(parent.error());
        auto control=SearchFieldTestView::create(builder,*parent,"search",options,
            {.edited=[&](std::string text){edits.push_back(std::move(text));}});
        if(!control)return std::unexpected(control.error());field=*control;return field->root().set_frame({0,0,220,22});
    }));
    auto original_drawing=take(tree->drawing_snapshot({400,100}));
    const auto surface=[](const ii::gui::CanvasFrame& frame)->const ii::gui::CanvasLayer&{
        const auto found=std::find_if(frame.overlays.begin(),frame.overlays.end(),[](const auto& layer){return layer.backdrop.has_value();});
        if(found==frame.overlays.end())throw std::runtime_error("Search field material was not drawn");
        return *found;
    };
    const auto& material=surface(original_drawing);
    require(std::abs(material.backdrop->blur_radius-8)<.001&&material.backdrop->capture&&
            material.backdrop->source==ii::gui::BackdropSource::previous_layers&&material.images.size()==1&&
            material.images.front().image,
            "Search field did not submit its Figma backdrop and material image");
    const auto first_material=material.images.front().image;
    auto repeated_drawing=take(tree->drawing_snapshot({400,100}));
    require(surface(repeated_drawing).images.front().image==first_material,
            "An unchanged search material was needlessly rerasterized");
    auto editor=find(tree,"search.editor"),icon=find(tree,"search.search"),clear=find(tree,"search.clear");
    auto bounds=take(field->root().snapshot()).bounds;
    auto icon_bounds=take(icon.snapshot()).bounds;
    require(std::abs(bounds.width-220)<.01&&std::abs(bounds.height-22)<.01&&
            std::abs(icon_bounds.width-12)<.01&&take(editor.snapshot()).accessibility_name=="Search sizes"&&
            take(clear.snapshot()).accessibility_name=="Clear search"&&!take(clear.snapshot()).visible,
            "Search field geometry, semantics, or initial clear visibility changed");
    good(editor.focus());
    auto rounded=take(field->options());
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(take(clear.snapshot()).visible&&edits.empty(),"Marked text did not expose clear without committing an edit");
    auto capsule=take(field->options());capsule.surface=lvui::InputFieldSurface::cylinder;good(field->set_options(capsule));
    auto capsule_frame=take(tree->drawing_snapshot({400,100}));
    require(take(field->snapshot()).marked.has_value()&&std::abs(surface(capsule_frame).backdrop->radius-11)<.001,
            "Cylinder search lost its Figma geometry or marked text");
    bool capsule_focus=false;
    for(const auto& layer:capsule_frame.overlays)for(const auto& panel:layer.panels)
        if(std::abs(panel.bounds.width-220)<.01&&std::abs(panel.bounds.height-22)<.01&&panel.radius==11)
            capsule_focus=true;
    require(capsule_focus,"Cylinder search focus outline did not follow its material");
    capsule.surface=lvui::InputFieldSurface::inline_;good(field->set_options(capsule));
    auto inline_frame=take(tree->drawing_snapshot({400,100}));
    require(take(field->snapshot()).marked.has_value()&&surface(inline_frame).backdrop->radius==0&&
            std::abs(surface(inline_frame).backdrop->blur_radius-6)<.001,
            "Inline search lost the Figma material or marked text");
    auto unsupported=rounded;unsupported.surface=lvui::InputFieldSurface::flat;
    require(!field->set_options(unsupported)&&take(field->snapshot()).marked.has_value(),
            "Invalid search surface changed the active composition");
    good(field->set_options(rounded));
    good(clear.activate());
    require(!take(clear.snapshot()).visible&&take(field->snapshot()).text.empty()&&edits.empty()&&take(editor.snapshot()).focused,
            "Clearing uncommitted text changed the committed model or lost focus");
    good(tree->input({.kind=WidgetInputKind::text,.text="A4"}));
    require(take(clear.snapshot()).visible&&edits==std::vector<std::string>{"A4"},"Typing did not update the search field");
    const auto button=take(clear.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=button.x+button.width/2,.y=button.y+button.height/2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=button.x+button.width/2,.y=button.y+button.height/2}));
    require(take(field->snapshot()).text.empty()&&edits==std::vector<std::string>({"A4",""})&&take(editor.snapshot()).focused,
            "Pointer clear did not deliver one edit and preserve focus");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A4"&&take(clear.snapshot()).visible&&edits.back()=="A4",
            "Undo did not restore the search query and clear action");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field->snapshot()).text.empty()&&!take(clear.snapshot()).visible&&edits.back().empty(),"Redo did not clear the query");
    good(tree->input({.kind=WidgetInputKind::text,.text="A"}));
    auto model_options=take(field->options());model_options.text="A";good(field->set_options(model_options));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    good(field->set_options(model_options));
    require(take(field->snapshot()).text=="A한"&&take(field->snapshot()).marked.has_value(),
            "An unchanged external value replaced the active composition");
    const auto before_clear=edits.size();
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=button.x+button.width/2,.y=button.y+button.height/2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=button.x+button.width/2,.y=button.y+button.height/2}));
    require(edits.size()==before_clear+1&&edits.back().empty()&&take(editor.snapshot()).focused,
            "Pointer clear committed marked text separately or moved focus");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A"&&!take(field->snapshot()).marked,
            "Undo after clearing composition did not restore the pre-composition query");
    model_options.placeholder="Updated placeholder";good(field->set_options(model_options));
    require(take(field->snapshot()).text=="A","Restyling replaced the locally edited query");
    good(field->set_text("A very long query"));
    good(field->root().set_frame({0,0,100,22}));
    (void)take(tree->drawing_snapshot({400,100}));
    const auto resized=take(clear.snapshot()).bounds,leading=take(icon.snapshot()).bounds;
    require(resized.x>=leading.x+leading.width&&resized.x+resized.width<=100.01&&resized.width>0,
            "Resizing overlapped or displaced the clear action");
    auto invalid=take(field->options());invalid.width=-1;
    require(!field->set_options(invalid)&&take(field->snapshot()).text=="A very long query",
            "Invalid options changed the search field");
    const auto enabled_material=surface(take(tree->drawing_snapshot({400,100}))).images.front().image;
    auto parent=find(tree,"parent");
    good(parent.set_enabled(false));
    (void)take(tree->drawing_snapshot({400,100}));
    require(!take(clear.snapshot()).visible&&std::abs(take(icon.snapshot()).opacity-.3)<.001&&
            !take(editor.snapshot()).enabled&&take(field->snapshot()).text=="A very long query",
            "An ancestor's disabled state did not update the search controls");
    const auto disabled_frame=take(tree->drawing_snapshot({400,100}));
    require(std::ranges::none_of(disabled_frame.overlays,[](const auto& layer){return layer.backdrop.has_value();}),
            "An ancestor's disabled state retained the Figma backdrop blur");
    good(parent.set_enabled(true));
    const auto restored_material=surface(take(tree->drawing_snapshot({400,100}))).images.front().image;
    require(std::ranges::equal(restored_material->rgba(),enabled_material->rgba()),
            "Re-enabling an ancestor did not restore the enabled search material");
    require(take(clear.snapshot()).visible&&std::abs(take(icon.snapshot()).opacity-1)<.001&&
            take(editor.snapshot()).enabled&&take(field->snapshot()).text=="A very long query",
            "Re-enabling an ancestor did not restore the search controls");
    auto current=take(field->options());current.read_only=true;good(field->set_options(current));
    require(!take(clear.snapshot()).visible&&take(field->snapshot()).text=="A very long query",
            "Read-only search exposed a destructive clear action");
    current.read_only=false;current.enabled=false;good(field->set_options(current));
    require(!take(clear.snapshot()).visible&&std::abs(take(icon.snapshot()).opacity-.3)<.001,
            "Disabled search exposed a clear action or retained its enabled icon opacity");
    good(field->root().set_frame({0,0,20,22}));
    (void)take(tree->drawing_snapshot({400,100}));
    require(!take(clear.snapshot()).visible&&!take(icon.snapshot()).visible,
            "An undersized field exposed overlapping or clipped icon controls");
    bool wrong_thread=false;std::thread worker([&]{auto r=field->set_text("bad");wrong_thread=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"A foreign thread changed the search field");
    tree->close();require(!field->set_text("retired"),"A retired search field accepted text");
}
void retirement_during_clear(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,80}));
    std::optional<SearchFieldTestView> field;unsigned edits=0;
    lvui::SearchFieldConfiguration options;options.text="query";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=SearchFieldTestView::create(builder,{},"search",options,
            {.edited=[&](std::string text){if(text.empty()){++edits;good(host.clear_content());}}},
            {.editor="app-search",.search_icon="app-search-icon",.clear="app-clear"});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    good(field->editor().focus());
    require(find(tree,"app-search")==field->editor(),"The caller's editor identity was not preserved");
    good(find(tree,"app-clear").activate());
    require(edits==1&&!field->root().snapshot(),"A clear callback did not safely retire the search field");
    tree->close();
}
void clear_glyph(){
    using namespace ii::gui;
    const auto fallback=take(ii::gui::input_field_clear_drawing(
        {.size={12,12}},lvui::search_field_defaults::description,lvui::search_field_defaults::background,
        lvui::search_field_defaults::font_size,lvui::search_field_defaults::clear_line_height,
        lvui::search_field_defaults::font_family,false));
    require(fallback->panels.size()==1&&fallback->panels[0].radius>0&&fallback->texts.size()==1&&
            fallback->texts[0].text=="×"&&fallback->texts[0].font_family!="SF Pro",
            "The portable clear action has no visible circle and cross without SF Pro");
#if defined(__APPLE__)
    TextRun mark{{0,0,12,16},"\xF4\x80\x81\xA1",{1,1,1,1},13,400,TextAlignment::center,"SF Pro"};
    auto symbol=take(render_text(mark,2));
    mark.text="\xF4\x8F\xBF\xBF";
    auto missing=take(render_text(mark,2));
    const auto visible=[](const TextBitmap& image){for(std::size_t i=3;i<image.rgba.size();i+=4)if(image.rgba[i])return true;return false;};
    require(visible(symbol)&&symbol.rgba!=missing.rgba,
            "The SF Pro clear symbol is missing or resolves to the missing-glyph box");
#endif
}
void material_raster(){
    using namespace ii::gui;
    SearchFieldTestMaterial material;
    WidgetPaintState paint{.size={220,22},.backing_scale=2};
    auto drawing=take(material.paint(paint,true));
    require(drawing->backdrop&&drawing->images.size()==1,"Search material has no native backdrop and raster");
    const auto image=drawing->images.front().image;
    require(image->width()==440&&image->height()==44,"Search material ignored display scale");
    const auto pixels=image->rgba();
    const auto at=[&](unsigned x,unsigned y,unsigned channel){return pixels[(std::size_t(y)*image->width()+x)*4+channel];};
    require(at(0,0,3)==0&&at(image->width()/2,image->height()/2,3)>0,
            "Search material did not clip its rounded corners");
    require(at(image->width()/2,0,0)<at(image->width()/2,image->height()/2,0)&&
            at(image->width()/2,image->height()-1,0)>at(image->width()/2,0,0),
            "Search material lost its top/bottom gradient and inner shadows");
    require(take(material.paint(paint,true))->images.front().image==image,
            "Search material did not retain its unchanged raster");
    auto cylinder=take(material.paint(paint,true,99));
    require(cylinder->backdrop&&std::abs(cylinder->backdrop->radius-11)<.001&&
            !std::ranges::equal(cylinder->images.front().image->rgba(),image->rgba()),
            "Text field material did not change its cached radius for the cylinder shape");
    paint.size={240,240};
    require(std::abs(take(material.paint(paint,true,99))->backdrop->radius-99)<.001,
            "Cylinder material lost the fixed Figma radius on a tall field");
    paint.size={220,22};
    require(!material.paint(paint,true,-1),"Negative text field material radius was accepted");
    using MaterialVariant=lvui::native::text_field_material::Variant;
    auto inline_enabled=take(material.paint(paint,true,0,MaterialVariant::inline_));
    require(inline_enabled->backdrop&&inline_enabled->backdrop->radius==0&&
            std::abs(inline_enabled->backdrop->blur_radius-6)<.001&&
            inline_enabled->images.front().image->rgba()[3]>0,
            "Inline material did not use the Figma blur and square geometry");
    auto disabled_drawing=take(material.paint(paint,false));
    require(!disabled_drawing->backdrop,"Disabled Figma material retained its backdrop blur");
    auto disabled=disabled_drawing->images.front().image;
    require(disabled!=image&&!std::ranges::equal(disabled->rgba(),image->rgba()),
            "Disabled search material did not use its disabled fill token");
    // Figma LVUI TextField/Disabled (114:174), transparent PNG export at 2x.
    // These samples are in the empty center-right area, outside all text and icons.
    paint.size={206,22};
    const auto reference=take(material.paint(paint,false))->images.front().image->rgba();
    constexpr std::array<unsigned,11> rows{0,1,2,4,8,14,22,30,37,41,43};
    constexpr std::array<std::array<int,4>,11> figma{{
        {{52,52,52,102}},{{55,55,55,102}},{{56,56,56,101}},{{53,53,53,101}},
        {{54,54,54,100}},{{58,58,58,97}},{{56,56,56,95}},{{59,59,59,95}},
        {{58,58,58,96}},{{60,60,60,97}},{{62,62,62,98}}}};
    for(std::size_t i=0;i<rows.size();++i)for(std::size_t c=0;c<4;++c)
        require(std::abs(int(reference[(std::size_t(rows[i])*412+280)*4+c])-figma[i][c])<=6,
                "Disabled material differs from the captured Figma reference samples");
    const auto inline_disabled=take(material.paint(paint,false,0,MaterialVariant::inline_));
    require(!inline_disabled->backdrop,"Disabled inline material retained the Figma blur");
    const auto inline_pixels=inline_disabled->images.front().image->rgba();
    constexpr std::array<std::array<int,4>,11> inline_figma{{
        {{53,53,53,58}},{{57,57,57,58}},{{59,59,59,56}},{{56,56,56,55}},
        {{57,57,57,54}},{{68,68,68,49}},{{68,68,68,45}},{{72,72,72,46}},
        {{71,71,71,47}},{{80,80,80,48}},{{83,83,83,49}}}};
    for(std::size_t i=0;i<rows.size();++i)for(std::size_t c=0;c<4;++c)
        require(std::abs(int(inline_pixels[(std::size_t(rows[i])*412+280)*4+c])-inline_figma[i][c])<=6,
                "Disabled inline material differs from the captured Figma reference samples");
    paint.size={8192,22};paint.backing_scale=8;
    auto bounded=take(material.paint(paint,true))->images.front().image;
    require(bounded->width()<=2048&&bounded->height()<=2048&&
            std::uint64_t(bounded->width())*bounded->height()<=1050624,
            "Search material exceeded its rasterization budget");
    paint.size={0,22};require(take(material.paint(paint,true))->images.empty(),"An empty search surface allocated a raster");
    paint.size.width=std::numeric_limits<double>::quiet_NaN();
    require(!material.paint(paint,true),"A non-finite search surface was accepted");
}
}
int main(){search_field();retirement_during_clear();clear_glyph();material_raster();}
