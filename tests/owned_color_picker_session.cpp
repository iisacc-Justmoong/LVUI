#include <LVUI/ColorPickerSession.h>
#include <LVUI/ColorSelectionSurface.h>

#include <memory>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::ColorSurfaceResult<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
class Substitute final : public lvui::ColorEditing {
public:
    explicit Substitute(lvui::StyleColor initial) : selection(initial) {}
    lvui::ColorSourceResult color() const override { return selection.color(); }
    std::uint64_t revision() const noexcept override { return selection.revision(); }
    lvui::ColorSelectionSnapshot snapshot() const override { return selection.snapshot(); }
    lvui::ColorSelectionResult<lvui::ColorEditToken> begin() override { return selection.begin(); }
    lvui::ColorSelectionResult<lvui::ColorSelectionSnapshot> begin_if_current(std::uint64_t revision) override {
        if (replace_before_begin) { replace_before_begin = false; (void)selection.set_color({0, 0, 1, 1}); }
        return selection.begin_if_current(revision);
    }
    lvui::ColorSelectionResult<std::uint64_t> edit(lvui::ColorEditToken token, lvui::StyleColor color) override {
        return selection.edit(token, color);
    }
    lvui::ColorSelectionResult<std::uint64_t> edit_if_current(std::uint64_t revision, lvui::StyleColor color, bool representation) override {
        return selection.edit_if_current(revision, color, representation);
    }
    lvui::ColorSelectionResult<lvui::ColorSelectionSnapshot> complete(lvui::ColorEditToken token, bool accepted) override {
        return selection.complete(token, accepted);
    }
    bool replace_before_begin = false;
    lvui::ColorSelection selection;
};
class RacingSurface final : public lvui::ColorSurfaceSource {
public:
    RacingSurface(std::shared_ptr<Substitute> editor, std::shared_ptr<lvui::ColorSelectionSurface> source, bool fail = false)
        : editor_(std::move(editor)), source_(std::move(source)), fail_(fail) {}
    lvui::ColorSourceResult color() const override {
        if (replace_) { replace_ = false; (void)editor_->selection.set_color({0, 0, 1, 1}); }
        if (fail_) return std::unexpected(lvui::ColorSourceError{"Source changed during reading"});
        return source_->color();
    }
    std::uint64_t revision() const noexcept override { return source_->revision(); }
    lvui::ColorSurfaceResult<lvui::ColorSurfaceFrame> frame(lvui::ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const override {
        return source_->frame(kind, width, height);
    }
    lvui::ColorSurfaceResult<std::uint64_t> edit(lvui::ColorSurfaceKind kind, lvui::ColorSurfacePoint point, std::uint64_t revision) override {
        return source_->edit(kind, point, revision);
    }
private:
    std::shared_ptr<Substitute> editor_;
    std::shared_ptr<lvui::ColorSelectionSurface> source_;
    mutable bool replace_ = true;
    bool fail_ = false;
};
class RacingFrame final : public lvui::ColorSurfaceSource {
public:
    RacingFrame(std::shared_ptr<Substitute> editor, std::shared_ptr<lvui::ColorSelectionSurface> source)
        : editor_(std::move(editor)), source_(std::move(source)) {}
    lvui::ColorSourceResult color() const override { return source_->color(); }
    std::uint64_t revision() const noexcept override { return source_->revision(); }
    lvui::ColorSurfaceResult<lvui::ColorSurfaceFrame> frame(lvui::ColorSurfaceKind, std::uint32_t, std::uint32_t) const override {
        (void)editor_->selection.set_color({0, 0, 1, 1});
        return std::unexpected(lvui::ColorSurfaceError{lvui::ColorSurfaceErrorCode::unavailable, "Source changed during rendering"});
    }
    lvui::ColorSurfaceResult<std::uint64_t> edit(lvui::ColorSurfaceKind kind, lvui::ColorSurfacePoint point, std::uint64_t revision) override {
        return source_->edit(kind, point, revision);
    }
private:
    std::shared_ptr<Substitute> editor_;
    std::shared_ptr<lvui::ColorSelectionSurface> source_;
};
}

int main() {
    using namespace lvui;
    const StyleColor red{1, 0, 0, .5}, blue{0, 0, 1, 1};
    auto editor = std::make_shared<Substitute>(red);
    auto picker = std::make_shared<ColorPicker>(editor);
    auto source = std::make_shared<ColorSelectionSurface>(editor);
    ColorPickerSession session(picker, source, source);
    require(take(session.frame(ColorSurfaceKind::wheel, 20, 20)).color == red &&
            take(session.channel_frame(ColorChannel::red)).value == 255 && !picker->active(),
            "Reading picker ports unexpectedly started editing");
    require(!session.edit_channel(ColorChannel::red, 256, 0) && !picker->active(),
            "Invalid picker input started an edit");
    const auto changed = take(session.edit_channel(ColorChannel::green, 128, 0));
    require(changed == editor->revision() && picker->active() && editor->snapshot().baseline == red &&
            editor->snapshot().color.green > .5, "Channel input did not start one shared edit session");
    require(!session.edit(ColorSurfaceKind::wheel, {.5, .5}, 0), "A stale gesture changed the picker");
    require(bool(picker->cancel()) && editor->snapshot().color == red, "Picker cancel did not restore a channel edit");
    const auto before = editor->revision(); editor->replace_before_begin = true;
    auto raced = session.edit(ColorSurfaceKind::wheel, {.5, .5}, before);
    require(!raced && raced.error().code == ColorSurfaceErrorCode::stale && !picker->active() &&
            editor->snapshot().color == blue, "A concurrent replacement was captured as a stale edit baseline");
    const auto next = editor->revision();
    (void)take(session.edit(ColorSurfaceKind::brightness_rail, {.5, 1}, next));
    require(picker->active() && editor->snapshot().baseline == blue, "Surface input did not start an edit session");
    require(bool(picker->cancel()) && editor->snapshot().color == blue, "Surface cancel did not restore its baseline");
    auto different = std::make_shared<ColorSelection>(red);
    auto wrong = std::make_shared<ColorSelectionSurface>(different);
    ColorPickerSession mismatched(picker, wrong, source);
    require(!mismatched.frame(ColorSurfaceKind::wheel, 20, 20) && !picker->active(),
            "Picker accepted ports from different color states");
    auto config = picker->configuration(); config.enabled = false;
    require(picker->set_configuration(config) && !session.edit_channel(ColorChannel::red, 0, editor->revision()) && !picker->active(),
            "Disabled picker accepted an edit");
    bool rejected = false;
    std::thread worker([&] { rejected = !session.prepare(editor->revision()); }); worker.join();
    require(rejected, "Picker accepted a foreign-thread edit");
    auto race_editor = std::make_shared<Substitute>(red);
    auto race_picker = std::make_shared<ColorPicker>(race_editor);
    auto race_source = std::make_shared<ColorSelectionSurface>(race_editor);
    auto racing = std::make_shared<RacingSurface>(race_editor, race_source);
    ColorPickerSession race_session(race_picker, racing, race_source);
    auto race = race_session.prepare(0);
    require(!race && race.error().code == ColorSurfaceErrorCode::stale && !race_picker->active() &&
            race_editor->snapshot().color == blue, "A source changed during measurement without reporting a stale revision");
    auto unavailable_editor = std::make_shared<Substitute>(red);
    auto unavailable_picker = std::make_shared<ColorPicker>(unavailable_editor);
    auto unavailable_source = std::make_shared<ColorSelectionSurface>(unavailable_editor);
    auto unavailable = std::make_shared<RacingSurface>(unavailable_editor, unavailable_source, true);
    ColorPickerSession unavailable_session(unavailable_picker, unavailable, unavailable_source);
    auto unavailable_race = unavailable_session.prepare(0);
    require(!unavailable_race && unavailable_race.error().code == ColorSurfaceErrorCode::stale,
            "A stale source failure hid a concurrent color replacement");
    auto frame_editor = std::make_shared<Substitute>(red);
    auto frame_picker = std::make_shared<ColorPicker>(frame_editor);
    auto frame_source = std::make_shared<ColorSelectionSurface>(frame_editor);
    auto frame_race = std::make_shared<RacingFrame>(frame_editor, frame_source);
    ColorPickerSession frame_session(frame_picker, frame_race, frame_source);
    auto interrupted = frame_session.frame(ColorSurfaceKind::wheel, 20, 20);
    require(!interrupted && interrupted.error().code == ColorSurfaceErrorCode::stale,
            "A concurrent replacement during rasterization was not reported as stale");
}
