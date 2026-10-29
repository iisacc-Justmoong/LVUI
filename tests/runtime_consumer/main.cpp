#include <LVUI/NativeWork.h>
#include <iostream>
int main(){
    ii::runtime::Runtime workers({.workers=1,.capacity=8});ii::runtime::EventLoop ui(16);
    lvui::native::WorkRuntime runtime(workers,ui.sender());auto activation=runtime.factories().activate();
    auto owner=std::make_shared<int>(0);auto context=activation.get<lvui::native::WorkGroup>()->scope("test");activation.attach(owner);
    if(!context->post([&](auto){(void)context->deliver([&]{*owner=42;});})||!workers.wait_idle())return 1;
    ui.poll({64,std::chrono::seconds(1)});if(*owner!=42)return 2;
    std::weak_ptr<int> retired=owner;activation.retire(owner);owner.reset();runtime.poll();
    if(!retired.expired()||!runtime.join())return 3;
    std::cout<<"Relocated LVUI::Runtime consumer passed\n";
}
