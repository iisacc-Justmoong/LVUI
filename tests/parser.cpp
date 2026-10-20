#include <LVUI/Document.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
lvui::Document good(std::string_view source) {
    auto result = lvui::parse(source);
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void bad(std::string_view source, std::string_view reason) {
    auto result = lvui::parse(source);
    check(!result, "Invalid document accepted");
    check(result.error().message.find(reason) != std::string::npos, "Wrong diagnostic");
    check(result.error().offset <= source.size(), "Diagnostic outside input");
}
void syntax() {
    auto doc = good("<!-- view --><main id='root'><h1>안녕 &amp; hello &#x1F44B;</h1><div><text> a\n b &lt;c&gt; </text><view /></div></main>");
    check(doc.root().id == "root" && doc.root().children.size() == 2, "Owned container tree");
    check(doc.root().children[0].text == "안녕 & hello 👋", "UTF-8, entities and text");
    check(doc.root().children[1].children[0].text == "a b <c>", "Whitespace normalization");
    check(doc.root().children[0].kind == lvui::Kind::text, "Text leaf");
    std::string temporary = "<view><p>owned</p></view>";
    auto owned = good(temporary); temporary.assign(temporary.size(), 'x');
    check(owned.root().children[0].text == "owned", "No dangling source views");
    check(good("<text>&quot;&apos;&nbsp;&#10;</text>").root().text == "\"' ", "Named entities and trailing whitespace");
}
void styles() {
    auto doc = good(R"(<view style="display:flex; flex-direction:row; width:100%; height:200px; padding:1px 2px 3px 4px; gap:8px; border-radius:12px; background:#1234; color:#aabbccdd; font-size:24px; font-weight:bold; text-align:center;"><p style='width:42; height:auto; background-color:transparent'/></view>)");
    const auto& s = doc.root().style;
    check(s.direction == lvui::Direction::row && s.width.unit == lvui::Unit::percent && s.width.value == 100, "Axis and percent");
    check(s.padding.left == 4 && s.padding.bottom == 3 && s.gap == 8 && s.radius == 12, "Box properties");
    check(std::abs(s.background->alpha - 4.f/15) < 0.0001f && s.font_weight == 700 && s.font_size == 24, "Colors and inherited text style");
    check(doc.root().children[0].style.width.value == 42, "Unitless logical pixels");
    check(good("<view style='padding:2px 4px 6px'/>").root().style.padding.left == 4, "Three-value padding");
    check(good("<view style='background:black; background:#fff'/>").root().style.background->red == 1, "Last declaration wins");
}
void diagnostics() {
    bad("", "root"); bad("<view/><view/>", "root");
    bad("<view><p>x</view>", "Closing"); bad("<view>", "Unclosed");
    bad("<script>work()</script>", "Unsupported tag"); bad("<style/>", "Unsupported tag");
    bad("<view onclick='x'/>", "Unsupported attribute"); bad("<view style='position:absolute'/>", "Unsupported style");
    bad("<view style='width:10vw'/>", "length"); bad("<view style='width:-1px'/>", "length");
    bad("<view style='width:nan'/>", "length"); bad("<view style='width:101%'/>", "length");
    bad("<view style='color:url(https://x)'/>", "color");
    bad("<view style='font-size:0'/>", "font-size"); bad("<view style='padding:1 2 3 4 5'/>", "padding");
    bad("<view id='x' id='y'/>", "Duplicate attribute");
    bad("<view><p id='a'/><p id='a'/></view>", "Duplicate id");
    bad("<view>mixed<p>x</p></view>", "text inside"); bad("<p>a<span>b</span></p>", "Text elements");
    bad("<text>&unknown;</text>", "entity"); bad("<text>&#xD800;</text>", "entity");
    bad("<text>&#0;</text>", "entity"); bad("<text>&amp</text>", "entity");
    bad("<view id=x/>", "quoted"); bad("<view id='unterminated", "Unclosed");
    bad("<!-- unclosed", "comment"); bad("<view/>tail", "root");
    bad(std::string("<text>\0</text>", 14), "NUL"); bad("<text>\xc0\x80</text>", "UTF-8");
    auto result = lvui::parse("<view>\n  <p>한글</p>\n  <bad/>\n</view>");
    check(!result && result.error().line == 3 && result.error().column == 4, "Precise line/column");
    auto scalar = lvui::parse("<text>한&ampX</text>");
    check(!scalar && scalar.error().column == 8, "Unicode scalar columns");
}
void limits() {
    bad(std::string(1024*1024+1, ' '), "1 MiB");
    std::string depth;
    for (int i=0;i<64;++i) depth += "<view>";
    for (int i=0;i<64;++i) depth += "</view>";
    (void)good(depth); bad("<view>"+depth+"</view>", "depth");
    std::string count = "<view>";
    for (int i=0;i<4095;++i) count += "<view/>";
    (void)good(count+"</view>"); bad(count+"<view/></view>", "4096");
    // Every truncated prefix must either parse completely or return a bounded diagnostic.
    const std::string sample = "<view style='padding:8px'><p>한글 &amp; text</p></view>";
    for (std::size_t n=0;n<sample.size();++n) {
        auto result = lvui::parse(std::string_view(sample).substr(0,n));
        check(!result && result.error().offset<=n, "Truncation handling");
    }
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc>1 ? argv[1] : "syntax";
        if (name=="syntax") syntax(); else if (name=="styles") styles();
        else if (name=="diagnostics") diagnostics(); else if (name=="limits") limits();
        else throw std::runtime_error("Unknown case");
        std::cout << "LVUI " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
