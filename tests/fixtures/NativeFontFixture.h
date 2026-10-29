#pragma once
#include <LVRS/FontPolicy.h>
struct NativeFontProvider final:ii::gui::FontProvider {
    std::vector<std::string> names{"System","Arial"};
    std::vector<std::string> families()const override{return names;}
    std::string system_family()const override{return "System";}
    std::string resolve(const ii::gui::FontFace& face)const override{return face.family;}
    ii::gui::FontLoadReport load(std::span<const ii::gui::FontResource> resources)override{ii::gui::FontLoadReport result;for(const auto& resource:resources)result.resources.push_back({std::string(resource.file),false,"Not installed"});return result;}
};
struct NativeFontBackend final {
    static inline int live=0;
    std::shared_ptr<NativeFontProvider> provider=std::make_shared<NativeFontProvider>();
    std::shared_ptr<ii::gui::FontDatabase> fonts=std::make_shared<ii::gui::FontDatabase>(provider,ii::gui::FontFace{"System",400,"Regular"});
    lvrs::FontPolicy policy{fonts};
    unsigned file_revision=0;
    NativeFontBackend(){++live;}~NativeFontBackend(){--live;}
    void install(){provider->names={"System","Arial","Pretendard"};policy.refresh();}
    void enforce(){(void)policy.enforceApplicationFallback();}
};
