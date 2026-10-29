#include <LVUI/ColorPicker.h>

#include <cmath>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(lvui::ColorSelectionResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
class Substitute final : public lvui::ColorEditing {
public:
    explicit Substitute(lvui::StyleColor value) : selection(value) {}
    lvui::ColorSourceResult color() const override { return selection.color(); }
    std::uint64_t revision() const noexcept override { return selection.revision(); }
    lvui::ColorSelectionSnapshot snapshot() const override { return selection.snapshot(); }
    lvui::ColorSelectionResult<lvui::ColorEditToken> begin() override { return selection.begin(); }
    lvui::ColorSelectionResult<lvui::ColorSelectionSnapshot> begin_if_current(std::uint64_t revision) override { return selection.begin_if_current(revision); }
    lvui::ColorSelectionResult<std::uint64_t> edit(lvui::ColorEditToken token, lvui::StyleColor value) override { return selection.edit(token, value); }
    lvui::ColorSelectionResult<std::uint64_t> edit_if_current(std::uint64_t revision, lvui::StyleColor value, bool representation_changed) override {
        if (replace_before_hex) { replace_before_hex = false; (void)selection.set_color({0, 0, 1, 1}); }
        return selection.edit_if_current(revision, value, representation_changed);
    }
    lvui::ColorSelectionResult<lvui::ColorSelectionSnapshot> complete(lvui::ColorEditToken token, bool accepted) override {
        auto result = selection.complete(token, accepted);
        if (result && replace_after_completion) { replace_after_completion = false; (void)selection.set_color({0, 0, 1, 1}); }
        return result;
    }
    bool replace_before_hex = false, replace_after_completion = false;
    lvui::ColorSelection selection;
};
}

int main() {
    const lvui::StyleColor red{1, 0, 0, .5}, blue{0, 0, 1, 1};
    auto editor = std::make_shared<Substitute>(red);
    lvui::ColorPicker picker(editor);
    require(!picker.active() && picker.configuration().mode == lvui::ColorPickerMode::wheel,
            "Color picker defaults changed");
    (void)take(picker.begin());
    require(!picker.begin() && !picker.set_hex("#XYZ123") && editor->selection.snapshot().color == red,
            "Picker accepted an overlapping session or invalid HEX input");
    (void)take(picker.set_hex("#102030"));
    const lvui::StyleColor edited{16. / 255, 32. / 255, 48. / 255, .5};
    require(editor->selection.snapshot().color == edited, "Six-digit HEX did not preserve alpha");
    auto accepted = take(picker.accept());
    require(!picker.active() && accepted.color == edited && picker.configuration().recent_colors.front() == edited,
            "Picker did not atomically accept and remember the chosen color");
    (void)take(picker.begin()); (void)take(picker.set_hex("AABBCC80"));
    auto canceled = take(picker.cancel());
    require(canceled.color == edited && editor->selection.snapshot().color == edited,
            "Picker cancellation did not restore its edit baseline");
    (void)take(picker.begin()); (void)take(picker.set_color(red)); (void)take(editor->selection.set_color(blue));
    require(!picker.cancel() && !picker.active() && editor->selection.snapshot().color == blue,
            "A stale picker cancel overwrote a newer external color");
    (void)take(picker.begin()); editor->replace_before_hex = true;
    require(!picker.set_hex("ABCDEF") && !picker.active() && editor->selection.snapshot().color == blue,
            "A concurrent replacement was overwritten by a stale HEX edit");
    (void)take(picker.begin()); (void)take(picker.set_color(red)); editor->replace_after_completion = true;
    auto committed = take(picker.accept());
    require(committed.color == red && picker.configuration().recent_colors.front() == red &&
            editor->selection.snapshot().color == blue,
            "Picker reported a later external color as the accepted edit");
    auto replacement = std::make_shared<Substitute>(red);
    (void)take(picker.begin());
    require(!picker.set_editor(replacement), "Picker replaced an active editor without completing its session");
    (void)take(picker.cancel()); require(bool(picker.set_editor(replacement)), "Picker did not accept a replaceable editor");
    const auto start = take(picker.begin_if_current(replacement->revision()));
    (void)take(picker.set_color(blue));
    require(!picker.set_color_if_current(start.revision, red) && !picker.set_hex_if_current(start.revision, "FF0000") &&
            replacement->snapshot().color == blue, "A stale picker value overwrote a newer edit in the same session");
    (void)take(picker.cancel());
    {
        lvui::ColorPicker temporary(replacement);
        (void)take(temporary.begin()); (void)take(temporary.set_color(blue));
    }
    require(replacement->selection.snapshot().color == red && !replacement->selection.snapshot().editing,
            "Retiring a picker left its edit session active");
    {
        lvui::ColorPicker stale(replacement);
        (void)take(stale.begin()); (void)take(stale.set_color(blue)); (void)take(replacement->selection.set_color(edited));
    }
    require(replacement->selection.snapshot().color == edited, "Retiring a stale picker reverted an external update");
    auto config = picker.configuration(); config.width = 100;
    require(!picker.set_configuration(config), "Invalid picker width was accepted");
    config = picker.configuration(); config.padding = 64; config.width = 320;
    require(!picker.set_configuration(config), "Picker accepted less than 228 content units");
    config = picker.configuration(); config.recent_colors.assign(11, red);
    require(!picker.set_configuration(config), "Picker accepted too many recent colors");
    require(lvui::ColorPicker::format_hex({1, 0, .5, 1}) == "#FF0080FF",
            "Picker did not format HEX consistently");
}
