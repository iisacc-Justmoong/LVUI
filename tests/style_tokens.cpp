#include <LVUI/Compiler.h>
#include <LVUI/Document.h>
#include <LVUI/Language.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
using namespace lvui;
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
template <class T> T take(StyleTokenResult<T> value) {
    if (!value)
        throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void near(double a, double b) { check(std::abs(a - b) < 1e-12, "Token precision changed"); }
const auto &tokens() { return default_style_tokens(); }
std::string compile(std::string_view source, NativeCompileOptions options = {}) {
    auto value = compile_native(source, options);
    if (!value)
        throw std::runtime_error(value.error().message);
    return *value;
}
void catalog() {
    check(tokens().tokens().size() == 208 && tokens().collections().size() == 5, "Incomplete Figma catalog");
    check(bool(tokens().validate()), "Default catalog is invalid");
    unsigned numbers = 0, colors = 0, strings = 0;
    std::set<std::string> names;
    for (const auto &token : tokens().tokens()) {
        check(names.insert(token.css_name).second, "CSS token collision");
        auto value = take(tokens().resolve(token.id));
        check(value == take(tokens().resolve(token.css_name)) && value == take(tokens().resolve(token.key)),
              "Identity lookup changed");
        check(token.type == style_token_type(value) && !token.id.empty() && !token.key.empty(),
              "Figma metadata/type missing");
        numbers += token.type == StyleTokenType::number;
        colors += token.type == StyleTokenType::color;
        strings += token.type == StyleTokenType::string;
    }
    check(numbers == 67 && colors == 138 && strings == 3, "Token types changed");
    check(take(tokens().string("Theme Typography/Font/fontBody")) == "Pretendard", "Font missing");
    near(take(tokens().number("--lvrs-theme-metrics-opacity-32")), 32);
    near(take(tokens().color("--lvrs-theme-colors-panel-window")).red, 0.0784313753247261);
    near(take(tokens().color("--lvrs-material-surface-dense-edge")).alpha, 0.11999999731779099);
}
void aliases() {
    unsigned count = 0;
    for (const auto &token : tokens().tokens())
        for (const auto &value : token.values)
            count += std::holds_alternative<StyleTokenAlias>(value.value);
    check(count == 6, "Figma alias identity was flattened");
    auto blue = take(tokens().color("Theme Colors/Accent/Blue/Default"));
    check(blue == take(tokens().color("Theme Colors/Action/primary")) &&
              blue == take(tokens().color("ColorPicker/Previous color")),
          "Transitive aliases failed");
    StyleTokenOptions options;
    options.overrides["Theme Colors/Accent/Blue/Default"] = StyleColor{.2, .3, .4, .5};
    check(take(tokens().color("ColorPicker/Previous color", options)) == StyleColor{.2, .3, .4, .5},
          "Alias ignored target override");
    options.overrides["Theme Colors/Action/primary"] = StyleColor{.7, .6, .5, 1};
    check(take(tokens().color("ColorPicker/Previous color", options)) == StyleColor{.7, .6, .5, 1},
          "Alias ignored intermediate override");
    check(take(tokens().color("Theme Colors/Accent/Blue/Default")) == blue,
          "Overrides mutated shared defaults");
}
StyleTokenCatalog sample(bool cycle = false) {
    return {
        {{"collection", "collection-key", "Sample", "dark", {{"dark", "Dark"}, {"light", "Light"}}, false}},
        {{"base",
          "base-key",
          "Base",
          "collection",
          "--base",
          "",
          StyleTokenType::number,
          {{"dark", StyleTokenValue{8.}}, {"light", StyleTokenValue{16.}}},
          {},
          false},
         {"alias",
          "alias-key",
          "Alias",
          "collection",
          "--alias",
          "",
          StyleTokenType::number,
          {{"dark", StyleTokenAlias{cycle ? "alias" : "base"}}, {"light", StyleTokenAlias{"base"}}},
          {},
          false}}};
}
void modes() {
    auto c = sample();
    StyleTokenOptions o;
    o.modes["Sample"] = "Light";
    near(take(c.number("--alias")), 8);
    near(take(c.number("--alias", o)), 16);
    o.modes.clear();
    o.modes["collection"] = "light";
    near(take(c.number("alias-key", o)), 16);
    check(bool(c.validate(o)), "Mode identity failed");
    o.modes["Sample"] = "Light";
    check(bool(c.validate(o)), "Equivalent mode name/id rejected");
    o.modes["Sample"] = "Dark";
    check(!c.validate(o), "Conflicting collection modes accepted");
    check(!sample(true).validate(), "Alias cycle accepted");
    auto bad = tokens().number("--lvrs-theme-colors-action-primary");
    check(!bad && bad.error().code == StyleTokenErrorCode::wrong_type, "Typed getter coerced a color");
}
void overrides() {
    StyleTokenOptions o;
    o.overrides["--lvrs-theme-metrics-space-8"] = 10.;
    check(bool(tokens().validate(o)), "Valid override rejected");
    near(take(tokens().number("--lvrs-theme-metrics-space-8", o)), 10);
    o.overrides["Theme Metrics/Space/8"] = 11.;
    check(!tokens().validate(o), "Duplicate override accepted");
    o.overrides.clear();
    o.overrides["--lvrs-theme-metrics-space-8"] = std::string("10");
    check(!tokens().validate(o), "Override coerced a string");
    o.overrides["--lvrs-theme-metrics-space-8"] = std::numeric_limits<double>::quiet_NaN();
    check(!tokens().validate(o), "NaN override accepted");
    o.overrides.clear();
    o.overrides["--missing"] = 1.;
    check(!tokens().validate(o), "Unknown override accepted");
    o.overrides.clear();
    o.modes["Theme Colors"] = "Light";
    check(!tokens().validate(o), "Unpublished mode invented");
    o.modes.clear();
    o.overrides["Theme Typography/Font/fontBody"] = std::string("\xff", 1);
    check(!tokens().validate(o), "Invalid UTF-8 String accepted");
}
void css() {
    check(!take(tokens().css_value("8px")), "Literal treated as var()");
    auto n = take(tokens().css_value(" var( --lvrs-theme-metrics-space-8 ) "));
    check(n && std::get<double>(*n) == 8, "CSS token lookup failed");
    auto fallback = take(tokens().css_value("var(--missing, var(--also-missing, 12px))"));
    check(fallback && std::get<double>(*fallback) == 12, "Nested typed fallback failed");
    auto c = take(tokens().css_value("var(--missing, rgba(255, 0, 128, 0.12))"));
    check(c && std::get<StyleColor>(*c).alpha == .12, "RGBA fallback failed");
    auto s = take(tokens().css_value("var(--missing, 'Pretendard')"));
    check(s && std::get<std::string>(*s) == "Pretendard", "String fallback failed");
    auto quoted = take(tokens().css_value("var(--missing, 'Font (Display)')"));
    check(quoted && std::get<std::string>(*quoted) == "Font (Display)",
          "Quoted parentheses changed CSS nesting");
    for (auto value :
         {"var(--missing)", "var(space-8)", "var(--missing,)", "var(--missing", "var(--x, var(--y)"})
        check(!tokens().css_value(value), "Malformed/unresolved CSS token accepted");
}
void composite() {
    auto edge = take(tokens().color("--lvrs-material-surface-dense-edge"));
    auto window = take(tokens().color("--lvrs-theme-colors-panel-window"));
    auto c = composite_srgb(edge, window);
    near(c.red, window.red + (1 - window.red) * edge.alpha);
    near(c.alpha, 1);
    check(c.red * 255 > 47 && c.red * 255 < 49, "Encoded sRGB composition changed");
    check(composite_srgb({0, 0, 0, 0}, {0, 0, 0, 0}) == StyleColor{0, 0, 0, 0},
          "Transparent composite became opaque");
}
void parser() {
    auto d = parse(
        R"LVUI(<main style="padding:var(--missing, 8px) var(--lvrs-theme-metrics-space-12); color:var(--lvrs-theme-colors-text-body-color)"><h1>LVUI</h1></main>)LVUI");
    check(bool(d), "Parser did not resolve tokens");
    check(d->root().style.padding.top == 8 && d->root().style.padding.left == 12,
          "CSS var padding split failed");
    check(d->root().style.font_size == 13 && d->root().children.front().style.font_size == 26,
          "Default typography not token based");
    check(d->root().style.background && std::abs(d->root().style.background->red - .0784313753) < 1e-7,
          "Default window color missing");
    StyleTokenOptions o;
    o.overrides["--lvrs-theme-typography-body-text-body"] = 19.;
    check(parse("<main/>", o)->root().style.font_size == 19, "Parser default override ignored");
    check(parse("<main style='font-size:22px'/>", o)->root().style.font_size == 22,
          "Explicit style did not win");
    o.overrides["--lvrs-theme-typography-body-text-body"] = -1.;
    check(!parse("<main/>", o), "Invalid default font size accepted");
    check(!parse("<main style='color:var(--lvrs-theme-metrics-space-8)'/>"),
          "Parser coerced number into color");
    check(!parse("<main style='font-weight:450'/>"), "Unsupported native font weight accepted");
}
void aot() {
    const auto source = R"(let ink = token("Theme Colors/Text/bodyColor")
ApplicationWindow(fontFamily: token("Theme Typography/Font/fontBody")) {
    VStack(spacing: token("--lvrs-theme-metrics-space-8")) {
        Label(text: "Native", color: ink, fontSize: token("Theme Typography/Body/textBody"))
        Label(text: "Title", style: .title)
    }
})";
    auto code = compile(source);
    check(code.find(".spacing = 8") != code.npos && code.find(".font_size = 13") != code.npos,
          "Typed AOT token numbers missing");
    check(code.find("result.font_family = \"Pretendard\"") != code.npos &&
              code.find("result.window = ii::gui::WidgetColor{") != code.npos,
          "Appearance defaults not compiled");
    check(code.find(".line_height = 26") != code.npos && code.find(".font_weight = 700") != code.npos,
          "Token typography recipe missing");
    check(code.find("token(") == code.npos && code.find("LVUI/") == code.npos,
          "Runtime token resolver leaked into app");
    NativeCompileOptions o;
    o.style_tokens.overrides["--lvrs-theme-metrics-space-8"] = 14.;
    check(compile(source, o).find(".spacing = 14") != code.npos, "AOT override ignored");
    auto html = compile(
        R"LVUI(<ApplicationWindow style="background:var(--lvrs-theme-colors-panel-window);font-family:var(--lvrs-theme-typography-font-font-body)"><div style="gap:var(--lvrs-theme-metrics-space-8)"><span style="font-size:var(--lvrs-theme-typography-body-text-body);color:var(--lvrs-theme-colors-action-primary)">Native</span></div></ApplicationWindow>)LVUI",
        o);
    check(html.find(".spacing = 14") != html.npos, "Markup lost compiler token options");
    auto composite = compile(
        R"(Window { Label(color: composite(token("--lvrs-material-surface-dense-edge"),token("--lvrs-theme-colors-panel-window"))) })");
    check(composite.find(".color = ii::gui::WidgetColor{") != composite.npos,
          "Explicit sRGB composition not compiled");
    auto panel = compile(
        R"(Window { VStack(backgroundColor: token("Theme Colors/Panel/window"),borderColor: token("--lvrs-material-surface-dense-edge"),borderWidth: token("Theme Metrics/Stroke/1")) {} })");
    check(panel.find("set_paint_handler") != panel.npos && panel.find(".stroke_width = 1") != panel.npos,
          "Token panel painting not generated");
    auto colors = compile(R"(Window { VStack {
      Popover(surfaceColor: token("--lvrs-theme-colors-panel-window")) {}
      ColorPickerButton(currentColor: token("--lvrs-color-picker-current-color"))
      ColorPicker(recentColors: [token("--lvrs-theme-colors-action-primary"), "#123456"])
    } })",
                          {.framework = "LVRS"});
    check(colors.find(".surface_color = ii::gui::WidgetColor{") != colors.npos &&
              colors.find(".current_color = ii::gui::WidgetColor{") != colors.npos &&
              colors.find("std::vector<ii::gui::WidgetColor>{ii::gui::WidgetColor{") != colors.npos,
          "Specialized color inputs rejected typed tokens");
    check(compile(source) == code, "Compilations shared mutable token overrides");
}
void diagnostics() {
    for (auto source :
         {R"(Window { Label(color: token("--missing")) })", R"(Window { Label(color: token(8)) })",
          R"(Window { Label(color: token("Theme Metrics/Space/8")) })",
          R"(Window { VStack(spacing: token("Theme Colors/Action/primary")) {} })",
          R"(Window { Label(style: "") })", R"(Window { VStack(borderWidth: 1) {} })",
          R"(Window { Label(fontWeight: 450) })",
          R"(Window { Label(color: composite(token("Theme Metrics/Space/8"),"#fff")) })",
          R"(let c = token("--missing")
Window { Label() })"})
        check(!compile_native(source), "Invalid AOT token accepted");
    auto error = compile_native("Window {\n Label(color: token(\"--missing\"))\n}");
    check(!error && error.error().line == 2, "Token diagnostic lost source location");
    NativeCompileOptions o;
    o.style_tokens.modes["Theme Colors"] = "Light";
    check(!compile_native("Window { Label() }", o), "Invalid unused mode accepted");
}
void selectors() {
    unsigned colors = 0, metrics = 0;
    for (const auto &token : tokens().tokens()) {
        if (token.type != StyleTokenType::color && token.type != StyleTokenType::number) {
            check(token.selector.empty(), "Non-style scalar acquired a typed selector");
            continue;
        }
        check(!token.selector.empty(), "Missing typed selector");
        check(take(tokens().select(token.selector)) == take(tokens().resolve(token.id)),
              "Typed selector changed token identity/value");
        colors += token.selector.starts_with("Color.");
        metrics += token.selector.starts_with("Metric.");
    }
    check(colors == 138 && metrics == 67, "Typed selector inventory incomplete");
    check(take(tokens().select("Color.primary")) == take(tokens().resolve("Theme Colors/Action/primary")),
          "Color primary alias changed");
    near(take(tokens().number("Metric.space8")), 8);
    near(take(tokens().number("Metric.stroke1_5")), 1.5);
    near(take(tokens().number("Metric.textBody")), 13);
    check(!tokens().select("Color.space8") && !tokens().select("Metric.primary"),
          "Selector crossed type domains");
    StyleTokenOptions o;
    o.overrides["Color.blue"] = StyleColor{.3, .4, .5, 1};
    check(take(tokens().color("Color.primary", o)) == StyleColor{.3, .4, .5, 1},
          "Selectors flattened Figma aliases");
    auto v = take(tokens().css_value("Metric.space8", o));
    check(v && std::get<double>(*v) == 8, "Inline selector missing");
}
void typed_syntax() {
    check(find_keyword("Color") && find_keyword("Color")->kind == KeywordKind::type,
          "Color is not a language type");
    check(find_keyword("Metric") && find_keyword("Metric")->kind == KeywordKind::type,
          "Metric is not a language type");
    auto source = R"(let ink: Color = .primary
let gap: Metric = .space8
Window { Label(color: Color.text.body, fontSize: Metric.textBody) })";
    auto d = parse_view(source);
    check(bool(d), "Typed language syntax rejected");
    check(d->bindings()[0].type == "Color" && d->bindings()[1].type == "Metric", "Type annotation lost");
    const auto &value = d->root().children.front().inputs.front().value;
    check(value.kind == ValueKind::style_member && value.text == "Color.text.body",
          "Style member became a backend reference");
    check(!parse_view("let Color = 1\nWindow { Label() }") &&
              !parse_view("import Metric Backend\nWindow { Label() }"),
          "Builtin types can be shadowed");
}
void typed_values() {
    auto explicit_code = compile(R"(let ink: Color = Color.primary
let gap: Metric = Metric.space8
Window(backgroundColor: Color.window) {
 VStack(spacing: gap, padding: Metric.space24, borderColor: Color.material.surface.dense.edge,
        borderWidth: Metric.stroke1, cornerRadius: Metric.radius8) {
   Label(text: "Typed", color: ink, fontSize: Metric.textBody, fontWeight: Metric.weightMedium)
 }
})");
    auto shorthand = compile(R"(let ink: Color = .primary
let gap: Metric = .space8
Window(backgroundColor: .window) {
 VStack(spacing: gap, padding: .space24, borderColor: .material.surface.dense.edge,
        borderWidth: .stroke1, cornerRadius: .radius8) {
   Label(text: "Typed", color: ink, fontSize: .textBody, fontWeight: .weightMedium)
 }
})");
    check(explicit_code == shorthand, "Contextual selectors differ from qualified selectors");
    auto old = compile("Window { Label(color: token(\"Theme Colors/Action/primary\"), fontSize: 13) }");
    check(old == compile("Window { Label(color: Color.primary, fontSize: Metric.textBody) }"),
          "Typed selection changed native output");
    auto custom = compile(R"(let ink: Color = Color("#12345678")
let gap: Metric = Metric(12)
Window { VStack(spacing: gap) { Label(color: ink) } })");
    check(custom.find(".spacing = 12") != custom.npos, "Metric construction failed");
    auto reactive =
        compile("var distance: Metric = .space8\nWindow { Slider(value: distance, from: 0, to: 20) }");
    check(reactive.find("Observable<double> value_distance{8}") != reactive.npos,
          "Metric lost numeric reactive state");
    check(reactive ==
              compile("var distance: Double = 8\nWindow { Slider(value: distance, from: 0, to: 20) }"),
          "Metric changed the native numeric binding contract");
    auto columns =
        compile("Window { TableHeader(columns: [\"A\",\"B\"], columnWidths: [Metric.size44,.size56]) }",
                {.framework = "LVRS"});
    check(columns.find(".column_widths = {44, 56}") != columns.npos, "Numeric list lost Metric context");
    auto html = compile(
        R"LVUI(<ApplicationWindow style="background:Color.window"><div style="padding:Metric.space24;gap:.space8"><span color={Color.text.body} style="font-size:Metric.textBody;font-weight:.weightMedium">Typed</span></div></ApplicationWindow>)LVUI");
    check(html.find(".spacing = 8") != html.npos && html.find(".font_size = 13") != html.npos,
          "Native inline typed style failed");
    auto legacy = parse(
        "<main "
        "style='background:Color.window;gap:Metric.space8;font-weight:.weightMedium;color:.text.body'/>");
    check(legacy && legacy->root().style.gap == 8 && legacy->root().style.font_weight == 500,
          "Initial parser typed style failed");
    NativeCompileOptions o;
    o.style_tokens.overrides["Metric.space8"] = 14.;
    check(compile("Window { VStack(spacing: .space8) {} }", o).find(".spacing = 14") != html.npos,
          "Typed selector override ignored");
}
void typed_diagnostics() {
    for (auto source :
         {"let x: Color = Metric.space8\nWindow { Label() }",
          "let x: Metric = Color.primary\nWindow { Label() }", "Window { Label(color: Metric.space8) }",
          "Window { VStack(spacing: Color.primary) {} }", "Window { Label(color: Color.missing) }",
          "Window { Label(color: .missing) }", "Window { Label(color: Color) }",
          "Window { Label(color: Metric(\"8\")) }", "Window { Label(color: Color(8)) }",
          "Window { Label(color: Color.primary()) }",
          "let untyped = .primary\nWindow { Label(color: untyped) }",
          "var title: String = \"bad\"\nlet gap: Metric = title\nWindow { Label() }"})
        check(!compile_native(source), "Invalid typed style accepted");
    auto d = compile_native("Window {\n Label(color: Color.missing)\n}");
    check(!d && d.error().line == 2, "Typed selector diagnostic lost source location");
    check(!parse("<main style='color:Metric.space8'/>"), "Inline color accepted Metric");
    check(!parse("<main style='padding:Color.primary'/>"), "Inline length accepted Color");
}
void profile() {
    auto p =
        compile_style_profile("let window: Color = Color(0.1,0.2,0.3,1)\nlet inspectorWidth: Metric = "
                              "352\nlet edge: Color = composite(.material.surface.dense.edge,.window)\n");
    check(bool(p), "Typed profile failed");
    near(take(tokens().number("Metric.inspectorWidth", *p)), 352);
    check(take(tokens().color("Color.window", *p)) == StyleColor{.1, .2, .3, 1},
          "Builtin profile override lost");
    check(take(tokens().color("Color.edge", *p)).alpha == 1, "Profile composition lost");
    NativeCompileOptions options;
    options.style_tokens = *p;
    auto code =
        compile("Window { VStack(width: Metric.inspectorWidth, backgroundColor: .window) {} }", options);
    check(code.find("set_stack_layout(") != code.npos && code.find(".extent = {352, 0}") != code.npos, "App Metric was not compiled");
    auto header = compile_style_header(*p, "Example.Theme");
    check(header && header->find("namespace Example::Theme") != header->npos &&
              header->find("inspectorWidth = 352") != header->npos &&
              header->find("#include <LVUI/") == header->npos,
          "Native style header is not standalone");
    for (auto source : {"var width: Metric = 12", "let wrong: Metric = Color.primary",
                        "import vm Backend\nlet x:Metric=8", "let invalid:Color=Color(2,0,0,1)"})
        check(!compile_style_profile(source), "Invalid style profile accepted");
    StyleTokenOptions invalid;
    invalid.members["Metric.unknown"] = std::string("8");
    check(!tokens().validate(invalid), "App member wrong type accepted");
    StyleTokenOptions base;
    base.overrides["Theme Colors/Panel/window"] = StyleColor{0, 0, 0, 1};
    auto layered = compile_style_profile("let window: Color = Color(0.1,0.2,0.3,1)", base);
    check(layered && layered->overrides.size() == 1, "Profile duplicated a builtin base override");
    check(take(tokens().color("Color.window", base)) == StyleColor{0, 0, 0, 1},
          "Profile mutated base options");
    auto collision = compile_style_profile("let textBody: Color = Color.primary");
    check(collision && !compile_style_header(*collision), "Flattened C++ member collision accepted");
    check(!compile_style_header(*p, "invalid;namespace"), "Invalid native namespace accepted");
    auto integral = compile_style_profile("let large: Metric = 1000000");
    auto integral_header = compile_style_header(*integral);
    check(integral_header && integral_header->find("large = 1000000;") != integral_header->npos,
          "Integral metric emitted as a floating exponent");
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string_view test = argc > 1 ? argv[1] : "catalog";
        if (test == "catalog")
            catalog();
        else if (test == "aliases")
            aliases();
        else if (test == "modes")
            modes();
        else if (test == "overrides")
            overrides();
        else if (test == "css")
            css();
        else if (test == "composite")
            composite();
        else if (test == "parser")
            parser();
        else if (test == "aot")
            aot();
        else if (test == "diagnostics")
            diagnostics();
        else if (test == "selectors")
            selectors();
        else if (test == "typed_syntax")
            typed_syntax();
        else if (test == "typed_values")
            typed_values();
        else if (test == "typed_diagnostics")
            typed_diagnostics();
        else if (test == "profile")
            profile();
        else
            throw std::runtime_error("Unknown case");
        std::cout << "LVUI style tokens " << test << " passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
