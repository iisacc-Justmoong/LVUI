include(CheckCXXSourceCompiles)
function(lvui_require_core_observation)
    if(NOT TARGET iiSDKCore::Observation)
        message(FATAL_ERROR "LVUI_CORE_OBSERVATION_REQUIRED: the matching Core Observation execution component is missing")
    endif()
    set(CMAKE_REQUIRED_LIBRARIES iiSDKCore::Observation)
    set(CMAKE_CXX_STANDARD 23)
    unset(LVUI_CORE_SCOPED_OBSERVATION CACHE)
    check_cxx_source_compiles("#include <iiSDKCore/Core/Property/Observable.h>
int main() {
    ii::core::Observable<int> source{1};
    auto read = source.read_dependency();
    return ii::core::ObservableReadScope::collect([](ii::core::ObservableRead) {}, [&] { return source.get(); }) - 1;
}" LVUI_CORE_SCOPED_OBSERVATION)
    if(NOT LVUI_CORE_SCOPED_OBSERVATION)
        message(FATAL_ERROR "LVUI_CORE_OBSERVATION_REQUIRED: install the matching iiSDKCore Observation component and headers with scoped Observable reads")
    endif()
endfunction()
lvui_require_core_observation()
