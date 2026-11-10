#include <LVUI/NativeBindings.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace lvui::native;
    BindingDiagnostics diagnostics;
    auto parent=std::make_shared<Bindings>(),child=std::make_shared<Bindings>();
    const auto parent_id=take(diagnostics.attach(parent)),child_id=take(diagnostics.attach(child));
    require(parent_id!=child_id&&take(diagnostics.activation_count())==2,"Binding activation identities were not unique");
    require(!diagnostics.attach(child),"A graph was registered twice");
    ii::core::Observable<int> value{1};
    const auto binding=take(child->bind([&](ReadContext& context){return context.read(value);},[](int next)->ii::core::Result<void>{
        if(next<0)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"negative child value");return {};
    }));
    value.set(-1);const auto failures=take(diagnostics.failures());
    require(failures.size()==1&&failures.front().activation==child_id&&failures.front().failure.binding==binding&&
            failures.front().failure.error.code==ii::core::ErrorCode::invalid_argument,
            "A nested binding failure was not attributed to its activation");
    value.set(2);require(take(diagnostics.failures()).empty(),"A recovered child remained in diagnostics");
    value.set(-1);good(diagnostics.detach(child_id));
    require(take(diagnostics.failures()).empty()&&take(diagnostics.activation_count())==1,
            "A detached activation retained its failure");
    const auto retained=take(diagnostics.attach(child));require(retained!=child_id,"A reused activation identity was accepted");
    good(child->close());require(take(diagnostics.failures()).empty()&&take(diagnostics.activation_count())==1,
            "A closed graph remained in diagnostics");
    require(!diagnostics.attach(child),"A closed graph was registered");
    auto transient=std::make_shared<Bindings>();std::weak_ptr<Bindings> weak=transient;(void)take(diagnostics.attach(transient));transient.reset();
    require(weak.expired()&&take(diagnostics.activation_count())==1,"Diagnostics retained an otherwise retired graph");
    std::shared_ptr<Bindings> foreign;std::thread creator([&]{foreign=std::make_shared<Bindings>();});creator.join();
    auto wrong_owner=diagnostics.attach(foreign);
    require(!wrong_owner&&wrong_owner.error().code==ii::core::ErrorCode::wrong_executor,"A graph from another owner thread was not identified correctly");
    bool rejected=false;std::thread worker([&]{auto result=diagnostics.failures();rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"A foreign thread queried binding diagnostics");
    good(diagnostics.close());require(!diagnostics.failures()&&!diagnostics.attach(parent),"Closed diagnostics accepted work");
}
