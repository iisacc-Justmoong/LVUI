#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <utility>
#include <vector>

struct ReadingListItem final {
    std::string id;
    std::string title;
    friend bool operator==(const ReadingListItem&, const ReadingListItem&) = default;
};

// One backend is owned by each activation of the view's `import vm` scope.
struct ReadingListBackend final {
    ii::core::Observable<std::string> draft{""};
    ii::core::Observable<std::vector<ReadingListItem>> items{
        std::vector<ReadingListItem>{{"1", "Learn the basics"}, {"2", "Build a native view"}}};
    ii::core::Observable<bool> hasItems{true};
    unsigned nextId = 3;

    void edited(std::string value) { draft.set(std::move(value)); }
    void add() {
        auto title = draft.get();
        if (title.empty())
            return;
        auto next = items.get();
        next.push_back({std::to_string(nextId++), std::move(title)});
        items.set(std::move(next));
        hasItems.set(true);
        draft.set("");
    }
    void clear() {
        items.set(std::vector<ReadingListItem>{});
        hasItems.set(false);
    }
};
