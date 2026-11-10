#pragma once
#include <iiGUI/NativeView.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>

struct NativeEmbeddedHostBackend final {
    ii::core::Observable<std::shared_ptr<ii::gui::NativeViewSource>> source{nullptr};
};
