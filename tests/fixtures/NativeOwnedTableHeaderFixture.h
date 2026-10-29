#pragma once
#include <LVUI/Table.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <string>
#include <vector>

struct NativeOwnedTableHeaderModel final {
    ii::core::Observable<std::vector<std::string>> columns{std::vector<std::string>{"First","Second","Third"}};
    ii::core::Observable<bool> enabled{true},visible{true};
    ii::core::Observable<double> width{300};
    std::vector<std::string> clicked;
    void click(std::size_t,const lvui::TableColumn& column){clicked.push_back(column.title);}
};
