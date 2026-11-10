#pragma once
#include <LVUI/ColorSource.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

class NativeWindowAccentColorSource final : public lvui::ColorSource {
    lvui::StyleColor color_{1, 1, 0, 1};
    std::uint64_t revision_ = 1;
public:
    [[nodiscard]] lvui::ColorSourceResult color() const override { return color_; }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return revision_; }
    void set(lvui::StyleColor value) { color_ = value; ++revision_; }
};

struct NativeWindowAccentExtrasModel final {
    ii::core::Observable<std::string> accent{std::string("blue")};
    ii::core::Observable<double> width{640};
    ii::core::Observable<double> progress{50};
    std::shared_ptr<NativeWindowAccentColorSource> color_owner = std::make_shared<NativeWindowAccentColorSource>();
    ii::core::Observable<std::shared_ptr<const lvui::ColorSource>> color_source{color_owner};
    ii::core::Observable<std::int64_t> color_revision{1};
    int alerts = 0, colors = 0, tools = 0, dialog_opens = 0, dialog_closes = 0, dialog_actions = 0;
    void alert() { ++alerts; }
    void color() { ++colors; }
    void tool() { ++tools; }
    void dialogOpened() { ++dialog_opens; }
    void dialogClosed() { ++dialog_closes; }
    void dialogAction() { ++dialog_actions; }
};
