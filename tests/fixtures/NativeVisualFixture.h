#pragma once
#include <iiGUI/NativeView.h>
#include <iiSDKCore/Core/Property/Observable.h>
class NativeVisualSource final:public ii::gui::NativeViewSource {
public:
    inline static int live=0;
    mutable int builds=0,retires=0;
    bool fail=false;
    mutable std::vector<ii::gui::Widget> fields;
    NativeVisualSource(){++live;}
    ~NativeVisualSource(){--live;}
    ii::core::Result<void> build(ii::gui::WidgetBuilder& b)const override{
        ++builds;if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"C++ visual candidate rejected");
        auto root=b.vstack({},"panel");if(!root)return std::unexpected(root.error());
        auto field=b.input_field(*root,"field",{.text="Native draft"});if(!field)return std::unexpected(field.error());fields.push_back(*field);
        auto ink=b.absolute_stack(*root,"ink",{160,32});if(!ink)return std::unexpected(ink.error());
        ii::gui::WidgetDrawing drawing;drawing.panels.push_back({.bounds={0,0,160,32},.top={.2f,.4f,.8f,1},.bottom={.2f,.4f,.8f,1}});
        if(auto r=ink->set_drawing(std::move(drawing));!r)return r;
        return b.on_retire([this]{++retires;});
    }
};
class NativeVisualBackend final {
public:
    inline static int live=0;
    std::shared_ptr<NativeVisualSource> first=std::make_shared<NativeVisualSource>(),second=std::make_shared<NativeVisualSource>(),bad=std::make_shared<NativeVisualSource>();
    ii::core::Observable<std::shared_ptr<ii::gui::NativeViewSource>> source{first};
    ii::core::Observable<double> width{240},height{100};
    ii::core::Observable<bool> enabled{true},clip{true},shown{true};
    ii::core::Observable<std::string> caption{"Before"};
    int file_revision=0;
    NativeVisualBackend(){++live;bad->fail=true;}
    ~NativeVisualBackend(){--live;}
    void replace(){source.set(second);}
    void reject(){source.set(bad);}
    void recover(){source.set(first);}
    void resize(){width.set(320);}
    void rename(){caption.set("After");}
};
