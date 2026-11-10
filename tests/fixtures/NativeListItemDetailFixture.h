#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeListItemDetailModel final {
    ii::core::Observable<std::string> date{"Today"};
    ii::core::Observable<bool> dateVisible{true},foldersVisible{true},tagsVisible{true};
};
