#pragma once

#include <LVUI/TableSorting.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Replace TableMemory with application-owned data ports when persistence is needed.
struct InventoryBackend final {
    std::shared_ptr<lvui::TableMemory> data = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="name", .title="Item", .width=190},
                                       {.key="category", .title="Category", .width=160},
                                       {.key="status", .title="Status", .width=160}},
        std::vector<lvui::TableRow>{{.key="a", .cells={{.text="Notebook"}, {.text="Tools"}, {.text="Ready"}}},
                                    {.key="b", .cells={{.text="Camera"}, {.text="Equipment"}, {.text="In use"}}},
                                    {.key="c", .cells={{.text="Microphone"}, {.text="Equipment"}, {.text="Ready"}}}});
    ii::core::Observable<std::shared_ptr<lvui::TableController>> controller{
        std::make_shared<lvui::TableController>(data)};
    ii::core::Observable<std::shared_ptr<lvui::TableEditor>> editor{data};
    ii::core::Observable<std::shared_ptr<lvui::TableOrderEditor>> order{
        std::make_shared<lvui::TableMemoryOrderEditor>(data)};
    ii::core::Observable<std::int64_t> revision{0};

    void changed() { revision.set(static_cast<std::int64_t>(data->snapshot().value()->revision)); }
    void submitted(std::size_t, std::size_t, std::string) { changed(); }
    void sorted(std::size_t, lvui::TableSortOrder) { changed(); }
    void pasted(const lvui::TableRange&) { changed(); }
};
