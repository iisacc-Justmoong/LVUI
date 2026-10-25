#include <LVUI/StyleTokens.h>
#include <iostream>

int main() {
    const auto& catalog = lvui::default_style_tokens();
    auto valid = catalog.validate();
    if (!valid || catalog.tokens().size() != 208)
        return 1;
    auto font = catalog.string("--lvrs-theme-typography-font-font-body");
    if (!font || *font != "Pretendard")
        return 2;
    lvui::StyleTokenOptions custom;
    custom.overrides["Metric.space8"] = 10.;
    auto changed = catalog.number("Metric.space8", custom);
    auto original = catalog.number("Theme Metrics/Space/8");
    if (!changed || *changed != 10 || !original || *original != 8)
        return 3;
    auto primary = catalog.select("Color.primary");
    if (!primary || !std::holds_alternative<lvui::StyleColor>(*primary))
        return 4;
    unsigned colors = 0, metrics = 0;
    for (const auto& token : catalog.tokens()) {
        colors += token.selector.starts_with("Color.");
        metrics += token.selector.starts_with("Metric.");
    }
    if (colors != 138 || metrics != 67)
        return 5;
    std::cout << "Installed LVUI catalog: 208 tokens, immutable overrides verified\n";
}
