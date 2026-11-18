#include <LVUI/NativeTable.h>
#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

int main(){
    using namespace ii::gui;
    auto data=std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="item",.title="Item",.width=190},{.key="category",.width=160},{.key="status",.width=160}},
        std::vector<lvui::TableRow>{{.key="row",.cells={{.text="Notebook"},{.text="Tools"},{.text="Ready"}}}});
    lvui::native::TableInput input;input.controller=std::make_shared<lvui::TableController>(data);
    input.appearance.width=560;input.appearance.height=260;
    input.appearance.header_typography=lvui::TableTypography{12,12,600};
    input.appearance.cell_typography=lvui::TableTypography{13,13,500};
    input.appearance.palette.header_text={.25,.5,.75,.6};
    input.appearance.palette.cell_text={.5,.25,.125,.8};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{390,500}));std::optional<lvui::native::NativeTable> table;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.padding=24,.alignment=StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());
        auto value=lvui::native::NativeTable::create(builder,*stack,"table",input);
        if(!value)return std::unexpected(value.error());table=*value;return {};
    }));
    const auto drawing=take(tree->drawing_snapshot({390,500}));
    const auto text=[&](std::string_view value)->const TextRun&{
        for(const auto& run:drawing.texts)if(run.text==value)return run;
        for(const auto& layer:drawing.overlays)for(const auto& run:layer.texts)if(run.text==value)return run;
        throw std::runtime_error("Table text run missing");
    };
    require(text("Item").font_size==12&&text("Item").line_height==12&&text("Item").font_weight==600,
            "Header typography did not reach the rendered table");
    require(text("Notebook").font_size==13&&text("Notebook").line_height==13&&text("Notebook").font_weight==500,
            "Cell typography did not reach the rendered table");
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    require(std::abs(text("Item").color.red-linear(.25))<1e-6&&std::abs(text("Item").color.alpha-.6)<1e-6&&
            std::abs(text("Notebook").color.red-linear(.5))<1e-6&&std::abs(text("Notebook").color.alpha-.8)<1e-6,
            "Table text role colors did not reach the rendered text");
    auto invalid=input;invalid.appearance.header_typography->font_weight=450;
    require(!table->set_options(invalid),"Invalid table typography was accepted");
    auto frame=take(table->frame());
    require(frame->width<=342.1,"Table viewport did not adopt the available width");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=54,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=54}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    const auto selected=input.controller->selection();
    require(selected&&*selected&&(*selected)->current.column=="status","Keyboard did not select the last column");
    frame=take(table->frame());
    const auto status=std::ranges::find_if(frame->columns,[](const auto& column){return column.index==2;});
    require(status!=frame->columns.end()&&status->offset>=-0.01&&status->offset+status->extent<=frame->width+0.01,
            "Keyboard selection did not reveal the complete last column");
    good(host.set_extent({720,500}));(void)take(tree->drawing_snapshot({720,500}));
    frame=take(table->frame());require(std::abs(frame->width-560)<0.1,"Table did not restore its authored width");
    tree->close();
}
