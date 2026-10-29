#pragma once
#include <LVRS/ModelAdapter.h>
#include <memory>
#include <stdexcept>
struct NativeModelRows final:lvrs::ItemModel {
    std::vector<std::string> rows{"First","Second"};
    int rowCount()const override{return int(rows.size());}
    int columnCount()const override{return 2;}
    std::map<int,std::string> roleNames()const override{return {{7,"label"}};}
    std::optional<ii::core::Value> data(int row,int column,int role)const override{
        if(role!=7)return {};return ii::core::Value(rows.at(std::size_t(row))+(column?" alternate":""));
    }
    void append(){rows.push_back("Added");if(auto result=publish(lvrs::ItemModelEvent::rows_inserted);!result)throw std::runtime_error(result.error().message);}
};
struct NativeModelSourceBackend final {
    static inline int live=0;
    std::shared_ptr<NativeModelRows> primary=std::make_shared<NativeModelRows>(),secondary=std::make_shared<NativeModelRows>();
    lvrs::ModelSource source;
    unsigned file_revision=0;
    NativeModelSourceBackend(){secondary->rows={"Replacement"};bind(primary);++live;}
    ~NativeModelSourceBackend(){--live;}
    void bind(const std::shared_ptr<NativeModelRows>& rows){if(auto result=source.setSource(std::shared_ptr<const lvrs::ItemModel>(rows));!result)throw std::runtime_error(result.error().message);}
    std::string firstLabel()const{return source.textValue(source.at(0),ii::core::Value("label"),"Unavailable");}
    void append(){primary->append();}
    void alternateColumn(){if(auto result=source.setColumn(1);!result)throw std::runtime_error(result.error().message);}
    void replace(){bind(secondary);}
};
