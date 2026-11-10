#pragma once
#include <LVUI/IconCatalog.h>
#include <LVUI/TableHeader.h>
#include <LVUI/TableText.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct NativeOwnedTableHeaderModel final {
    ii::core::Observable<std::vector<std::string>> columns{std::vector<std::string>{"First","Second","Third"}};
    ii::core::Observable<bool> enabled{true},visible{true};
    ii::core::Observable<double> width{300};
    ii::core::Observable<lvui::TableText> tableText{lvui::TableText{}};
    ii::core::Observable<std::optional<lvui::TableHeaderSort>> sort{std::nullopt};
    ii::core::Observable<std::vector<lvui::TableHeaderColumnAppearance>> columnAppearance{std::vector<lvui::TableHeaderColumnAppearance>{}};
    ii::core::Observable<std::string> defaultLeadingIcon{"generalchevronDown"};
    ii::core::Observable<std::shared_ptr<const lvui::IconSource>> iconSource{nullptr};
    ii::core::Observable<std::int64_t> iconRevision{0};
    std::vector<std::string> clicked;
    void click(std::size_t,const lvui::TableColumn& column){clicked.push_back(column.title);}
};
