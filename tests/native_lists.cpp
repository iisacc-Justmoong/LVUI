#include "NativeLists.generated.h"
#include <iostream>
#include <stdexcept>
int main(){try{
    using View=lvui_generated::NativeLists;
    auto scope=std::make_shared<View::Scope>();
    auto bindings=std::make_shared<lvui::native::Bindings>();
    auto good=[](bool value){if(!value)throw std::runtime_error("Native reactive list declaration contract failed");};
    good(bool(View::initialize_values(scope,bindings)));
    good(scope->inferred_numbers->get()==std::vector<double>{0,7}&&scope->inferred_initial->get()==std::vector<double>{0,1});
    scope->backend_0.first.set(9);
    good(scope->inferred_numbers->get()==std::vector<double>{9,7}&&scope->inferred_initial->get()==std::vector<double>{0,1});
    good(bool(scope->inferred_words->set({"B","A","C"})));
    good(scope->inferred_words->get()==std::vector<std::string>{"B","A","C"});
    good(bool(bindings->close()));
    std::cout<<"Native list declarations compiled and ran without a UI host\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
