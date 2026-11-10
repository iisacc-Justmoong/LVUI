#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace lvui::native {
struct TableViewSnapshot final {
    std::string row_key,column_key,text,value_type;
    std::size_t row_index=0,column_index=0,row_span=1,column_span=1;
    double x=0,y=0,width=0,height=0;
    bool header=false,enabled=true,editable=false,selected=false,current=false,sorted=false,descending=false;
    friend bool operator==(const TableViewSnapshot&,const TableViewSnapshot&)=default;
};

// Each visible key owns a separately observable lifetime. No application model
// or widget implementation is retained by the table context.
class TableViewContext final {
public:
    [[nodiscard]] const TableViewSnapshot& get()const{check();return value_->get();}
    [[nodiscard]] std::uint64_t revision()const{check();return value_->revision();}
    [[nodiscard]] ii::core::ObservableSubscription observe(std::function<void(const TableViewSnapshot&,std::uint64_t)> callback,bool initial=false)const{
        check();auto value=value_;return value->observe(std::move(callback),initial);
    }
    [[nodiscard]] ii::core::ObservableRead read_dependency()const{check();return value_->read_dependency();}
private:
    friend struct TableViewContextPort;
    explicit TableViewContext(TableViewSnapshot value):value_(std::make_shared<ii::core::Observable<TableViewSnapshot>>(std::move(value))){}
    void check()const{
        if(owner_!=std::this_thread::get_id())throw std::logic_error("Table View context requires its owner thread");
        if(!value_)throw std::logic_error("Table View context is closed");
    }
    void publish(TableViewSnapshot value){check();auto observable=value_;observable->set(std::move(value));}
    void close()noexcept{value_.reset();}
    const std::thread::id owner_=std::this_thread::get_id();
    std::shared_ptr<ii::core::Observable<TableViewSnapshot>> value_;
};
} // namespace lvui::native
