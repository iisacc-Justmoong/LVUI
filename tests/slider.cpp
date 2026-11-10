#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Slider(id: \"slider\", from: 0, to: vm.maximum, value: vm.value, stepSize: 1, width: vm.width, enabled: vm.enabled, onMoved: vm.moved) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeSlider.h>","lvui::native::SliderViewBinding<ii::gui::BasicSlider,ii::gui::BasicNativeSlider,ii::gui::CachedSvgDecoder>::create(","lvui::SliderConfiguration","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Slider lost ")+token);
    if(code->find("widgets.slider(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Slider depends on a prebuilt or legacy control");
    auto sized=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(size: .mini)\n Slider(size: vm.size)\n} }");
    if(!sized)throw std::runtime_error(sized.error().message);
    if(sized->find("lvui::apply_slider_size(value, lvui::SliderSize::mini)")==sized->npos||
       sized->find("lvui::slider_size(")==sized->npos)
        throw std::runtime_error("Generated Slider lost its static or reactive size preset");
    auto ticks=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(showTicks: true, tickCount: 11)\n Slider(showTicks: vm.ticks, tickCount: vm.tickCount)\n} }");
    if(!ticks)throw std::runtime_error(ticks.error().message);
    if(ticks->find("value.show_ticks = true")==ticks->npos||ticks->find("lvui::slider_tick_count(")==ticks->npos)
        throw std::runtime_error("Generated Slider lost its static or reactive tick inputs");
    auto centered=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(type: .centerBiasedTicks, showTicks: false)\n Slider(type: vm.kind)\n} }");
    if(!centered)throw std::runtime_error(centered.error().message);
    if(centered->find("lvui::SliderType::center_biased_ticks")==centered->npos||
       centered->find("lvui::slider_type(")==centered->npos||
       centered->find("value.show_ticks = false")==centered->npos||
       centered->find("value.show_ticks = false")<centered->find("lvui::SliderType::center_biased_ticks"))
        throw std::runtime_error("Generated Slider lost its static or reactive center-biased variant");
    auto filled=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(type: .filled, size: .small, showSymbol: false)\n Slider(type: vm.kind)\n} }");
    if(!filled)throw std::runtime_error(filled.error().message);
    const auto filled_type=filled->find("lvui::SliderType::filled");
    const auto filled_size=filled->find("lvui::SliderSize::small",filled_type);
    const auto explicit_symbol=filled->find("value.show_symbol = false",filled_size);
    if(filled_type==filled->npos||filled_size==filled->npos||explicit_symbol==filled->npos||
       filled->find("lvui::slider_type(")==filled->npos)
        throw std::runtime_error("Generated filled Slider lost its type, size, explicit symbol, or reactive input");
    auto segmented=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(type: .minMaxLabels, showMinMax: false)\n Slider(type: .segmented, segmentCount: 3, stepSize: 0, showTicks: false)\n Slider(type: vm.kind, segmentCount: vm.count)\n} }");
    if(!segmented)throw std::runtime_error(segmented.error().message);
    const auto labeled_type=segmented->find("lvui::SliderType::min_max_labels");
    const auto hidden_labels=segmented->find("value.show_min_max = false",labeled_type);
    const auto segmented_type=segmented->find("lvui::SliderType::segmented");
    if(labeled_type==segmented->npos||hidden_labels==segmented->npos||segmented_type==segmented->npos||
       segmented->find("value.show_ticks = false",segmented_type)==segmented->npos||
       segmented->find("value.automatic_segment_step = false",segmented_type)==segmented->npos||
       segmented->find("value.segment_count = lvui::slider_segment_count(")==segmented->npos||
       segmented->find("lvui::slider_type(")==segmented->npos)
        throw std::runtime_error("Generated labeled or segmented Slider lost a preset or explicit input");
    lvui::NativeCompileOptions filled_theme;filled_theme.style_tokens.overrides["Color.text.disabled"]=lvui::StyleColor{.1,.2,.3,.4};
    auto filled_color=lvui::compile_native("Window { Slider(type: .filled) }",filled_theme);
    if(!filled_color||filled_color->find("value.palette.inactive_fill = lvui::native::slider_color(ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})")==filled_color->npos)
        throw std::runtime_error("Generated filled Slider ignored the inactive fill token");
    auto inactive=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(type: .filled, active: false)\n Slider(active: vm.active, inactiveFillColor: vm.color)\n} }");
    if(!inactive||inactive->find("value.active = false")==inactive->npos||
       inactive->find("value.active = lvui::native::truth(")==inactive->npos||
       inactive->find("value.palette.inactive_fill = lvui::native::slider_color(")==inactive->npos)
        throw std::runtime_error("Slider lost its static or reactive inactive-fill inputs");
    const std::string key="value.palette.inactive_fill = ";
    const auto first=inactive->find(key),second=inactive->find(key,first+key.size()),third=inactive->find(key,second+key.size());
    if(first==inactive->npos||second==inactive->npos||third==inactive->npos)
        throw std::runtime_error("Slider lost its explicit inactive-fill color binding");
    lvui::NativeCompileOptions theme;theme.style_tokens.overrides["Color.text.caption"]=lvui::StyleColor{.25,.5,.75,1};
    auto colored=lvui::compile_native("Window { Slider(type: .centerBiased) }",theme);
    if(!colored||colored->find("value.palette.mark = lvui::native::slider_color(ii::gui::WidgetColor{0.25, 0.5, 0.75, 1})")==colored->npos)
        throw std::runtime_error("Slider marks ignored the selected caption token");
    theme.style_tokens.overrides["Color.text.body"]=lvui::StyleColor{.125,.25,.5,.75};
    auto endpoint_theme=lvui::compile_native("Window { Slider(showMinMax: true) }",theme);
    if(!endpoint_theme||endpoint_theme->find("value.palette.label = lvui::native::slider_color(ii::gui::WidgetColor{0.125, 0.25, 0.5, 0.75})")==endpoint_theme->npos)
        throw std::runtime_error("Slider endpoint labels ignored the selected body token");
    auto endpoint_colors=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(labelColor: \"#ff000080\")\n Slider(labelColor: vm.labelColor)\n} }",theme);
    if(!endpoint_colors||endpoint_colors->find("value.palette.label = lvui::native::slider_color(")==endpoint_colors->npos||
       endpoint_colors->find("value.palette.label = lvui::native::slider_color(ii::gui::WidgetColor{1, 0, 0,")==endpoint_colors->npos||
       endpoint_colors->find("lvui::native::color<ii::gui::WidgetColor>(")==endpoint_colors->npos)
        throw std::runtime_error("Slider lost its explicit or reactive endpoint label color");
    theme.style_tokens.overrides["Color.surfaceSolid"]=lvui::StyleColor{.125,.25,.5,.75};
    theme.style_tokens.overrides["Color.text.titleHeader"]=lvui::StyleColor{.75,.5,.25,.125};
    auto surface_theme=lvui::compile_native("Window { Slider() }",theme);
    if(!surface_theme||surface_theme->find("value.palette.track = lvui::native::slider_color(ii::gui::WidgetColor{0.125, 0.25, 0.5, 0.75})")==surface_theme->npos||
       surface_theme->find("value.palette.thumb = lvui::native::slider_color(ii::gui::WidgetColor{0.75, 0.5, 0.25, 0.125})")==surface_theme->npos)
        throw std::runtime_error("Slider track or thumb ignored the selected common token");
    auto surface_override=lvui::compile_native("Window { Slider(trackColor: \"#ff0000\", thumbColor: \"#00ff00\") }",theme);
    if(!surface_override)throw std::runtime_error(surface_override.error().message);
    const auto track_default=surface_override->find("value.palette.track = lvui::native::slider_color(ii::gui::WidgetColor{0.125, 0.25, 0.5, 0.75})");
    const auto track_override=surface_override->find("value.palette.track = lvui::native::slider_color(ii::gui::WidgetColor{1, 0, 0, 1})");
    const auto thumb_default=surface_override->find("value.palette.thumb = lvui::native::slider_color(ii::gui::WidgetColor{0.75, 0.5, 0.25, 0.125})");
    const auto thumb_override=surface_override->find("value.palette.thumb = lvui::native::slider_color(ii::gui::WidgetColor{0, 1, 0, 1})");
    if(track_default==surface_override->npos||track_override==surface_override->npos||track_override<track_default||
       thumb_default==surface_override->npos||thumb_override==surface_override->npos||thumb_override<thumb_default)
        throw std::runtime_error("Explicit Slider track or thumb color did not take precedence");
    theme.style_tokens.overrides["Color.material.alert.actionBorder"]=lvui::StyleColor{.125,.25,.5,.75};
    auto symbol_theme=lvui::compile_native("Window { Slider(showSymbol: true) }",theme);
    if(!symbol_theme||symbol_theme->find("value.palette.symbol = lvui::native::slider_color(ii::gui::WidgetColor{0.125, 0.25, 0.5, 0.75})")==symbol_theme->npos)
        throw std::runtime_error("The default Slider symbol ignored the selected material token");
    auto symbols=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(showSymbol: true, symbolName: \"sliderSymbolSun\")\n Slider(showSymbol: vm.symbol, symbolName: vm.symbolName)\n} }");
    if(!symbols)throw std::runtime_error(symbols.error().message);
    if(symbols->find("value.show_symbol = true")==symbols->npos||symbols->find("value.symbol_name =")==symbols->npos)
        throw std::runtime_error("Generated Slider lost its static or reactive symbol inputs");
    auto endpoints=lvui::compile_native("import vm Model\nWindow { VStack {\n Slider(showMinMax: true, showLabels: true, showEndpointIcons: true, minimumLabel: \"Low\", maximumLabel: \"High\", minimumIconName: \"generaladd\", maximumIconName: \"\", rightToLeft: true)\n Slider(showMinMax: vm.endpoints, showLabels: vm.labels, showEndpointIcons: vm.icons, minimumLabel: vm.minimumLabel, maximumLabel: vm.maximumLabel, minimumIconName: vm.minimumIcon, maximumIconName: vm.maximumIcon, rightToLeft: vm.rtl)\n} }");
    if(!endpoints)throw std::runtime_error(endpoints.error().message);
    for(auto token:{"value.show_min_max = true","value.show_labels = true","value.show_endpoint_icons = true",
                    "value.minimum_label = ","value.maximum_label = ","value.minimum_icon_name = ",
                    "value.maximum_icon_name = ","value.right_to_left = true"})
        if(endpoints->find(token)==endpoints->npos)throw std::runtime_error(std::string("Generated Slider lost ")+token);
    for(auto source:{"Window { Slider(from: 1, to: 0) }","Window { Slider(width: 1) }",
                     "Window { Slider(thumbDiameter: 0) }","Window { Slider(trackHeight: -1) }",
                     "Window { Slider(stepSize: -1) }","Window { Slider(size: .unknown) }",
                     "Window { Slider(size: .large, height: 20) }", "Window { Slider(tickCount: 1) }",
                     "Window { Slider(tickCount: 513) }", "Window { Slider(tickCount: 3.5) }",
                     "Window { Slider(symbolName: \"\") }", "Window { Slider(minimumIconName: \"  \") }",
                     "Window { Slider(type: .unknown) }", "Window { Slider(type: .filled, size: .regular, height: 18) }",
                     "Window { Slider(type: .segmented, segmentCount: 1) }",
                     "Window { Slider(type: .segmented, segmentCount: 513) }",
                     "Window { Slider(type: .segmented, segmentCount: 3.5) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Slider input was accepted");
    if(lvui::compile_native("Window { Slider(symbolName: \""+std::string(257,'a')+"\") }"))
        throw std::runtime_error("An oversized Slider symbol name was accepted");
    if(lvui::compile_native("Window { Slider(maximumIconName: \""+std::string(257,'a')+"\") }"))
        throw std::runtime_error("An oversized Slider endpoint icon name was accepted");
}
