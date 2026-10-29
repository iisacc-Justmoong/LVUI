#include <LVUI/NativeObjectInputs.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Record final {std::string title;int count=0;};
void failures(){
    auto initial=std::make_shared<const Record>(Record{"First",1});
    ii::core::Observable<std::shared_ptr<const Record>> selected{initial};
    lvui::native::Bindings graph;
    auto input=graph.derive([&](lvui::native::ReadContext& context){return lvui::native::object_input<Record>(context.read(selected));});
    check(input&&(*input)->get()==initial,"Initial native input was not preserved");
    selected.set({});auto errors=graph.failures();
    check(errors&&errors->size()==1&&(*input)->get()==initial,"A failed typed input lost its owning graph's diagnostic or previous value");
    auto replacement=std::make_shared<const Record>(Record{"Second",2});selected.set(replacement);
    check((*input)->get()==replacement&&graph.failures()->empty(),"A typed input could not recover its value and diagnostic");
    check(bool(graph.close()),"A typed input graph did not close");
    bool closed=false;try{(void)(*input)->get();}catch(const std::logic_error&){closed=true;}
    check(closed,"A retired typed input remained readable");
}
void ownership(){
    auto first=std::make_shared<const Record>(Record{"First",1});std::weak_ptr<const Record> previous=first;
    ii::core::Observable<std::shared_ptr<const Record>> selected{first};
    lvui::native::Bindings graph;
    auto input=graph.derive([&](lvui::native::ReadContext& context){return lvui::native::object_input<Record>(context.read(selected));});
    check(bool(input),"Owning input failed");first.reset();selected.set({});check(!previous.expired(),"Failed replacement discarded the last valid owner");
    selected.set(std::make_shared<const Record>(Record{"Next",2}));check(previous.expired(),"Successful replacement kept the obsolete owner");
    check(bool(graph.close()),"Owning input did not retire");
}
void handles(){
    const Record value{"Borrowed",3};auto reference=lvui::native::object_input<Record>(std::cref(value));auto pointer=lvui::native::object_input<Record>(&value);
    check(&reference.get()==&value&&pointer==&value,"Native borrowing copied the object");
    auto nested=std::make_shared<std::unique_ptr<const Record>>(std::make_unique<const Record>(Record{"Nested",4}));
    check(lvui::native::object_input<Record>(nested)==nested,"Nested owning handles changed identity");
    nested->reset();bool rejected=false;try{(void)lvui::native::object_input<Record>(nested);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"A nested null payload was accepted");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"failures";if(name=="failures")failures();else if(name=="ownership")ownership();else if(name=="handles")handles();else throw std::runtime_error("Unknown native object input test");std::cout<<"Native typed input "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
