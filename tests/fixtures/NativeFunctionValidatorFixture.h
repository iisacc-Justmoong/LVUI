#pragma once
#include <LVRS/TableValue.h>
#include <memory>
struct NativeFunctionValidatorBackend final {
    lvrs::TableValidation validate(std::string text,std::string type)const{return {.accepted=type=="string",.text=text,.value=std::move(text)};}
    auto makeValidator(){return [state=std::make_unique<int>(0)](std::string text,std::string type){++*state;return lvrs::TableValidation{.accepted=type=="string",.text=text,.value=std::move(text)};};}
};
