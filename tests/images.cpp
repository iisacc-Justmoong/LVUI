#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
void shape(){auto result=compile("import vm Model\nWindow { Image(source: vm.image(), width: 64, height: 48, fit: .Contain) }");check(result.find("#include <LVUI/NativeImage.h>")!=result.npos&&result.find("lvui::native::NativeImage::create(")!=result.npos&&result.find("lvui::ImageFit::contain")!=result.npos&&result.find("widgets.image(")==result.npos&&result.find("lvrs::")==result.npos,"LVUI must own image geometry and painting");}
void bindings(){auto result=compile("import vm Model\nWindow { Image(source: vm.image(), width: vm.width(), height: vm.height(), backgroundColor: vm.background()) }");check(result.find("return lvui::native::ImageInput{")!=result.npos&&result.find(".width = lvui::native::number(")!=result.npos&&result.find("component.set_options(value)")!=result.npos&&result.find("set_image_options")==result.npos&&result.find("lvrs::")==result.npos,"Independent image fields did not bind without widget reconstruction");}
void empty(){auto result=compile("Window { Image(fit: .Cover, circular: true, allowEmpty: true) }");check(result.find(".image = {}")!=result.npos&&result.find("lvui::ImageFit::cover")!=result.npos,"Empty asynchronous image placeholder was rejected");}
void diagnostics(){for(auto source:{"Window { Image(source: \"file.svg\") }","Window { Image(fit: .Unknown) }","Window { Image(unknown: true) }"}){auto result=lvui::compile_native(source);check(!result&&result.error().line>0&&result.error().column>0,"Invalid Image input lost source diagnostics");}auto result=lvui::compile_native("Window { Image() }");check(bool(result)&&result->find("lvrs::")==result->npos,"Default image path depends on LVRS");}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"shape";if(name=="shape")shape();else if(name=="bindings")bindings();else if(name=="empty")empty();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown native Image compiler case");std::cout<<"Image compiler "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
