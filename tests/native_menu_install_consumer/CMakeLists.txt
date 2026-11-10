cmake_minimum_required(VERSION 3.31)
project(LVUIMenuBundleConsumer LANGUAGES CXX)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI CONFIG REQUIRED)
lvui_generate(NAME NativeMenuBundles SOURCE NativeMenuBundles.lvui
    INCLUDES NativeMenuBundlesFixture.h OUTPUT_VARIABLE generated)
lvui_generate(NAME NativeMenuNestedComposition SOURCE NativeMenuNestedComposition.lvui
    INCLUDES NativeMenuBundlesFixture.h OUTPUT_VARIABLE generated_nested)
lvui_generate(NAME NativeTableStructureControls SOURCE NativeTableStructureControls.lvui
    INCLUDES NativeOwnedTableFixture.h OUTPUT_VARIABLE generated_table)
lvui_generate(NAME NativeTabDynamic SOURCE NativeTabDynamic.lvui
    INCLUDES NativeTabDynamicFixture.h OUTPUT_VARIABLE generated_tabs)
lvui_generate(NAME NativeOwnedSearchField SOURCE NativeOwnedSearchField.lvui
    INCLUDES NativeOwnedSearchFieldFixture.h OUTPUT_VARIABLE generated_search)
lvui_generate(NAME NativeOwnedSlider SOURCE NativeOwnedSlider.lvui
    INCLUDES NativeOwnedSliderFixture.h OUTPUT_VARIABLE generated_slider)
lvui_generate(NAME NativeFocusRing SOURCE NativeFocusRing.lvui
    INCLUDES NativeFocusRingFixture.h OUTPUT_VARIABLE generated_focus)
lvui_generate(NAME NativeAbstractInputBar SOURCE NativeAbstractInputBar.lvui
    INCLUDES NativeAbstractInputBarFixture.h OUTPUT_VARIABLE generated_input_bar)
lvui_generate(NAME NativeExplicitListItemComposite SOURCE NativeExplicitListItemComposite.lvui
    OUTPUT_VARIABLE generated_composite)
lvui_generate(NAME NativeMobileTabs SOURCE NativeMobileTabs.lvui
    INCLUDES NativeMobileTabsFixture.h OUTPUT_VARIABLE generated_mobile_tabs)
lvui_generate(NAME NativeWindowNoise SOURCE NativeWindowNoise.lvui
    INCLUDES NativeWindowNoiseFixture.h OUTPUT_VARIABLE generated_noise)
lvui_generate(NAME NativeWheelGuard SOURCE NativeWheelGuard.lvui
    INCLUDES NativeWheelGuardFixture.h OUTPUT_VARIABLE generated_wheel)
add_executable(menu_bundle_consumer main.cpp "${generated}" "${generated_nested}" "${generated_table}" "${generated_tabs}" "${generated_search}" "${generated_slider}" "${generated_focus}" "${generated_input_bar}" "${generated_composite}" "${generated_mobile_tabs}" "${generated_noise}" "${generated_wheel}")
target_compile_features(menu_bundle_consumer PRIVATE cxx_std_23)
target_include_directories(menu_bundle_consumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(menu_bundle_consumer PRIVATE LVUI::Native iiGUI::iiGUI)
