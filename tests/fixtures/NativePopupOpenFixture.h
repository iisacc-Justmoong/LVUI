#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativePopupOpenModel final {
    ii::core::Observable<bool> modalOpen{true}, nestedOpen{true}, alertOpen{false}, sheetOpen{false};
    ii::core::Observable<bool> showConditional{false}, closeOnOpen{false};
    ii::core::Observable<bool> blockedOpen{false}, blockedVisible{false}, scopedOpen{false};
    ii::core::Observable<std::string> title{"Initial"};
    int modalOpens=0, modalCloses=0, nestedOpens=0, nestedCloses=0;
    int alertOpens=0, alertCloses=0, sheetOpens=0, sheetCloses=0, conditionalOpens=0;
    void modalOpened(){++modalOpens;if(closeOnOpen.get())modalOpen.set(false);}
    void modalClosed(){++modalCloses;}
    void nestedOpened(){++nestedOpens;}
    void nestedClosed(){++nestedCloses;}
    void alertOpened(){++alertOpens;}
    void alertClosed(){++alertCloses;}
    void sheetOpened(){++sheetOpens;}
    void sheetClosed(){++sheetCloses;}
    void conditionalOpened(){++conditionalOpens;}
};
