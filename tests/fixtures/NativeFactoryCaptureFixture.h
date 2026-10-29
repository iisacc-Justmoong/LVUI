#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace FactoryCaptureDomain {
struct Node final {
    static inline int live=0;
    struct Handler final {
        Node* parent;
        explicit Handler(Node* value):parent(value){}
        Handler(const Handler&)=delete;Handler(Handler&&)=delete;
        void operator()()&{parent->act();}
    } handler{this};
    struct Owned final {
        Node* parent;
        explicit Owned(Node* value):parent(value){}
        Owned(const Owned&)=delete;Owned(Owned&&)=delete;
        void operator()()&{parent->act();}
    };
    const std::string name;ii::core::Observable<int> count{0};std::function<void()> during;
    int factories=0;std::string last;
    explicit Node(std::string title):name(std::move(title)){++live;}~Node(){--live;}
    Node(const Node&)=delete;Node(Node&&)=delete;
    void act(){const auto next=count.get()+1;if(auto action=std::exchange(during,{}))action();count.set(next);}
    Handler& make(){++factories;return handler;}
    Owned value(){++factories;return Owned{this};}
    auto text(){return [this](std::string value){last=std::move(value);};}
    auto description(){return [this]{return name+" "+std::to_string(count.get());};}
};
struct Argument final {
    static inline int live=0;const std::string name;
    explicit Argument(std::string title):name(std::move(title)){++live;}~Argument(){--live;}
    Argument(const Argument&)=delete;Argument(Argument&&)=delete;
};
}
struct NativeFactoryCaptureBackend final {
    static inline int live=0;
    std::shared_ptr<FactoryCaptureDomain::Node> first=std::make_shared<FactoryCaptureDomain::Node>("First"),second=std::make_shared<FactoryCaptureDomain::Node>("Second");
    ii::core::Observable<std::shared_ptr<FactoryCaptureDomain::Node>> selected{first};
    ii::core::Observable<std::vector<std::shared_ptr<FactoryCaptureDomain::Node>>> rows{{first}};
    ii::core::Observable<bool> visible{true};ii::core::Observable<int> file_revision{0};
    std::string last;
    NativeFactoryCaptureBackend(){++live;}~NativeFactoryCaptureBackend(){--live;}
    FactoryCaptureDomain::Node temporary(){return FactoryCaptureDomain::Node{"Temporary"};}
    FactoryCaptureDomain::Argument argument(){return FactoryCaptureDomain::Argument{"Argument"};}
    auto capture(const FactoryCaptureDomain::Argument& argument){return [this,&argument]{last=argument.name;};}
    auto mixed(const FactoryCaptureDomain::Argument& borrowed,FactoryCaptureDomain::Argument value){
        return [this,&borrowed,name=value.name]{last=borrowed.name+"/"+name;};
    }
};
