#pragma once
#include <LVUI/List.h>
#include <LVUI/ListFooter.h>
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeOwnedListPanelModel final {
    std::shared_ptr<lvui::ListMemory> source=lvui::ListMemory::create({{.key="a",.label="Alpha"},{.key="b",.label="Beta"}}).value();
    ii::core::Observable<std::shared_ptr<lvui::ListController>> controller{
        std::make_shared<lvui::ListController>(lvui::ListController::create(source).value())};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<double> width{200};
    ii::core::Observable<bool> footer{true};
    int activations=0,tools=0,footers=0;
    void selected(std::size_t,const lvui::ListEntry&){++activations;}
    void tool(std::size_t,std::string){++tools;}
    void footerClicked(std::size_t,const lvui::ListFooterSlot&){++footers;}
};
