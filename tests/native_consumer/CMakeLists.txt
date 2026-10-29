cmake_minimum_required(VERSION 3.31)
project(LVUINativeConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
if(NOT LVUI_AOT_FRAMEWORKS STREQUAL "iiGUI")
    message(FATAL_ERROR "Installed LVUI must report its supported native AOT framework")
endif()
if(TARGET iiColorManager::iiColorManager)
    message(FATAL_ERROR "The Native contract imported the picker library")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE LVUI::Native)
set_target_properties(consumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME LVUI.NativeConsumer COMMAND consumer)
add_executable(compiler_consumer compiler.cpp)
target_link_libraries(compiler_consumer PRIVATE LVUI::LVUI)
set_target_properties(compiler_consumer PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME LVUI.CompilerConsumer COMMAND compiler_consumer)
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES LVUI::Native)
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){auto owner=std::make_shared<int>(0);auto graph=std::make_shared<lvui::native::Bindings>();
auto action=lvui::native::returned_callback(owner,graph,[](lvui::native::ReadContext&){},[](const auto&,auto& value){std::invoke(value);});action();}"
    LVUI_INVALID_VOID_CALLBACK_ACCEPTED)
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){auto owner=std::make_shared<int>(0);auto graph=std::make_shared<lvui::native::Bindings>();
auto action=lvui::native::returned_callback(owner,graph,[](lvui::native::ReadContext&){return 1;},[](const auto&,auto& value){std::invoke(value);});action();}"
    LVUI_INVALID_SCALAR_CALLBACK_ACCEPTED)
if(LVUI_INVALID_VOID_CALLBACK_ACCEPTED OR LVUI_INVALID_SCALAR_CALLBACK_ACCEPTED)
    message(FATAL_ERROR "A non-callable C++ callback result was accepted")
endif()
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){lvui::native::invoke_callback<void()>([]{return 1;});}"
    LVUI_INVALID_TYPED_CALLBACK_RETURN_ACCEPTED)
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){lvui::native::invoke_callback<void(std::string)>([](std::string){});}"
    LVUI_INVALID_TYPED_CALLBACK_ARITY_ACCEPTED)
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){lvui::native::invoke_callback<void(bool)>([](bool){},1);}"
    LVUI_INVALID_TYPED_CALLBACK_TYPE_ACCEPTED)
if(LVUI_INVALID_TYPED_CALLBACK_RETURN_ACCEPTED OR LVUI_INVALID_TYPED_CALLBACK_ARITY_ACCEPTED OR LVUI_INVALID_TYPED_CALLBACK_TYPE_ACCEPTED)
    message(FATAL_ERROR "An incompatible typed event callback was accepted")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)
set(CMAKE_REQUIRED_LIBRARIES LVUI::Native)
check_cxx_source_compiles("#include <LVUI/NativeComponentInputs.h>
struct Owner { std::shared_ptr<int> value = std::make_shared<int>(1); };
int main(){auto owner=std::make_shared<Owner>();
return *lvui::native::component_storage(owner,[](auto& scope){return scope.value;});}"
    LVUI_INVALID_COPIED_COMPONENT_STORAGE_ACCEPTED)
check_cxx_source_compiles("#include <LVUI/NativeComponentInputs.h>
struct Owner {};
int main(){auto owner=std::make_shared<Owner>();
lvui::native::component_storage(owner,[](auto& scope)->decltype((scope.value)){return (scope.value);});}"
    LVUI_INVALID_ESCAPED_COMPONENT_INPUT_ACCEPTED)
if(LVUI_INVALID_COPIED_COMPONENT_STORAGE_ACCEPTED OR LVUI_INVALID_ESCAPED_COMPONENT_INPUT_ACCEPTED)
    message(FATAL_ERROR "An invalid native component storage selector was accepted")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)
set(CMAKE_REQUIRED_LIBRARIES LVUI::Native)
check_cxx_source_compiles("#include <LVUI/NativeValues.h>
int main(){return lvui::native::invoke_callback<bool()>([]{return 1;});}"
    LVUI_INVALID_FUNCTION_BOOL_RESULT_ACCEPTED)
if(LVUI_INVALID_FUNCTION_BOOL_RESULT_ACCEPTED)
    message(FATAL_ERROR "A mismatched native function result was accepted")
endif()
unset(CMAKE_REQUIRED_LIBRARIES)
