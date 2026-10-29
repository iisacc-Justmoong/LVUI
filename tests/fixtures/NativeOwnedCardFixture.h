#pragma once
#include <LVUI/Card.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedCardModel final {
    ii::core::Observable<lvui::CardType> kind{lvui::CardType::project};
    ii::core::Observable<std::string> title{"Project"}, description{"Shared project"};
    ii::core::Observable<bool> selected{false}, enabled{true}, visible{true};
    ii::core::Observable<double> width{256}, progress{.5};
    std::vector<lvui::CardRow> rows{{"Owner", "User"}};
    int clicks=0, menus=0, actions=0;
    std::vector<bool> changes;
    void clicked(){++clicks;}
    void selection(bool value){changes.push_back(value);selected.set(value);}
    void menu(){++menus;}
    void action(){++actions;}
};
