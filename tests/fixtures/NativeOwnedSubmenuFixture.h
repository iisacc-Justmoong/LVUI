#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <vector>
struct NativeOwnedSubmenuModel final {
    int more=0,leaf=0,deep=0,opens=0,childCloses=0,parentCloses=0;
    std::vector<int> parentEvents,childEvents;
    ii::core::Observable<bool> moreEnabled{true},hasChildren{true};
    std::function<void()> onLeaf,onChildClosed;
    void moreClicked(){++more;}
    void leafClicked(){++leaf;auto callback=onLeaf;if(callback)callback();}
    void deepClicked(){++deep;}
    void childOpened(){++opens;}
    void childClosed(){++childCloses;auto callback=onChildClosed;if(callback)callback();}
    void parentClosed(){++parentCloses;}
    void parentTriggered(int value){parentEvents.push_back(value);}
    void childTriggered(int value){childEvents.push_back(value);}
};
