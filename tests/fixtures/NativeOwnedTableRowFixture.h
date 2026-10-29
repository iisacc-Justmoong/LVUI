#pragma once
#include <LVUI/TableRowView.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <string>
#include <vector>

struct NativeOwnedTableRowModel final {
    ii::core::Observable<std::vector<std::string>> cells{std::vector<std::string>{"First","Second","Third"}};
    ii::core::Observable<bool> enabled{true},visible{true};
    ii::core::Observable<double> width{350};
    std::vector<std::string> edits,submissions;
    void edited(std::size_t index,std::string value){edits.push_back(value);auto next=cells.get();next[index]=std::move(value);cells.set(std::move(next));}
    void submitted(std::size_t index,std::string value){submissions.push_back(std::to_string(index)+":"+value);}
};
