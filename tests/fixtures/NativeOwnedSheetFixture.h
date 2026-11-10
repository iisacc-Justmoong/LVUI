#pragma once
#include <iiGUI/NativeSheetView.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedSheetModel final {
    ii::core::Observable<std::string> title{"Export"}, text{"Document"};
    ii::core::Observable<double> width{560};
    ii::core::Observable<ii::gui::SheetPresentation> presentation{ii::gui::SheetPresentation::desktop};
    ii::core::Observable<ii::gui::SheetDetent> detent{ii::gui::SheetDetent::fit};
    int opens = 0, closes = 0, edits = 0;
    void opened() { ++opens; }
    void closed() { ++closes; }
    void edited(std::string) { ++edits; }
};
