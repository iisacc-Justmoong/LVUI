#include LVUI_EXAMPLE_HEADER
#include <iiGUI/Application.h>
#include <memory>

// iiSDKCore supplies main(), owns the event loop, and retires this attachment.
ii::core::Result<void> ii::runtime::configure_application(Application& app) {
    using View = lvui_generated::LVUI_EXAMPLE_VIEW;
    auto host = ii::gui::attach_application(app, View::options());
    if (!host)
        return std::unexpected(host.error());
    return app.attach(ii::gui::view_attachment("main", *host, std::make_shared<View>()));
}
