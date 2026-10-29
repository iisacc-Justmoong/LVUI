#pragma once
#include <LVUI/TableCellItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedTableCellItemModel final {
    ii::core::Observable<std::string> text{"1"};
    ii::core::Observable<bool> enabled{true},visible{true};
    ii::core::Observable<double> width{234};
    std::vector<std::string> edits,submissions,rejections;
    void edited(std::string value){edits.push_back(value);text.set(std::move(value));}
    void submitted(std::string value){submissions.push_back(std::move(value));}
    void rejected(std::string value,std::string){rejections.push_back(std::move(value));}
};
