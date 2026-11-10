#include <LVUI/CompilerBridge.h>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
lvui_compiler_bytes bytes(std::string_view value){return {value.data(),value.size()};}
using Result=std::unique_ptr<lvui_compiler_result,decltype(&lvui_compiler_result_free)>;
Result take(lvui_compiler_result* result){require(result!=nullptr,"Compiler bridge did not return a result");return {result,&lvui_compiler_result_free};}
std::string_view payload(const Result& value){return {lvui_compiler_result_data(value.get()),lvui_compiler_result_size(value.get())};}
}
int main(){
    require(lvui_compiler_abi_version()==1,"Unexpected compiler bridge ABI");
    auto output=take(lvui_compiler_compile(bytes("Window { Label() }"),bytes("BridgeView"),nullptr,0,nullptr));
    require(lvui_compiler_result_status(output.get())==LVUI_COMPILER_OK&&
            payload(output).find("struct BridgeView final")!=std::string_view::npos,
            "Compiler bridge did not return generated code");
    auto invalid=take(lvui_compiler_compile({nullptr,1},bytes("BridgeView"),nullptr,0,nullptr));
    require(lvui_compiler_result_status(invalid.get())==LVUI_COMPILER_INVALID_ARGUMENT,
            "Invalid source pointer was accepted");
    auto invalid_utf8=take(lvui_compiler_compile(bytes(std::string_view("\xff",1)),bytes("BridgeView"),nullptr,0,nullptr));
    require(lvui_compiler_result_status(invalid_utf8.get())==LVUI_COMPILER_INVALID_ARGUMENT,
            "Invalid UTF-8 crossed the compiler bridge");
    auto missing_includes=take(lvui_compiler_compile(bytes("Window { Label() }"),bytes("BridgeView"),nullptr,1,nullptr));
    require(lvui_compiler_result_status(missing_includes.get())==LVUI_COMPILER_INVALID_ARGUMENT,
            "Missing include array was accepted");
    auto too_many=take(lvui_compiler_compile(bytes("Window { Label() }"),bytes("BridgeView"),nullptr,257,nullptr));
    require(lvui_compiler_result_status(too_many.get())==LVUI_COMPILER_INVALID_ARGUMENT,
            "Excess includes were accepted");
    auto diagnostic=take(lvui_compiler_compile(bytes("Window { UnknownWidget() }"),bytes("BridgeView"),nullptr,0,nullptr));
    require(lvui_compiler_result_status(diagnostic.get())==LVUI_COMPILER_DIAGNOSTIC&&
            lvui_compiler_result_stage(diagnostic.get())==LVUI_COMPILER_SOURCE&&
            lvui_compiler_result_line(diagnostic.get())>=1&&!payload(diagnostic).empty(),
            "Compiler diagnostic lost its stage, location, or message");
    auto header=take(lvui_compiler_styles(bytes("let sample: Metric = 42"),bytes("Example.Theme")));
    require(lvui_compiler_result_status(header.get())==LVUI_COMPILER_OK&&
            payload(header).find("sample = 42")!=std::string_view::npos,
            "Compiler bridge did not return a style header");
}
