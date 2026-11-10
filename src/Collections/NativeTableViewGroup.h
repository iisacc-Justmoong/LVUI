#pragma once
#include <LVUI/TableViewContext.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace lvui::native {
template<class Builder>using TableViewFactoryValue=std::function<ii::core::Result<void>(Builder&,std::shared_ptr<const TableViewContext>)>;
template<class Builder>struct NativeTableViewsValue final {
    TableViewFactoryValue<Builder> header,cell;
    std::map<std::string,TableViewFactoryValue<Builder>,std::less<>> headers,columns;
};
template<class Rect>struct TableViewItemValue final {std::string key;TableViewSnapshot value;Rect frame;};
struct TableViewContextPort final {
    [[nodiscard]] static std::shared_ptr<TableViewContext> make(TableViewSnapshot value){
        return std::shared_ptr<TableViewContext>(new TableViewContext(std::move(value)));
    }
    static void publish(TableViewContext& context,TableViewSnapshot value){context.publish(std::move(value));}
    static void close(TableViewContext& context)noexcept{context.close();}
};
template<class Builder,class Rect>struct TableViewGroupPolicy final {
    using Snapshot=TableViewSnapshot;using Context=TableViewContext;using Factory=TableViewFactoryValue<Builder>;
    using Item=TableViewItemValue<Rect>;
    [[nodiscard]] static std::shared_ptr<Context> make_context(Snapshot value){return TableViewContextPort::make(std::move(value));}
    static void publish(Context& context,Snapshot value){TableViewContextPort::publish(context,std::move(value));}
    static void close(Context& context)noexcept{TableViewContextPort::close(context);}
};
template<template<class> class Engine,class Builder,class Rect>
using TableViewGroupBinding=Engine<TableViewGroupPolicy<Builder,Rect>>;
} // namespace lvui::native
