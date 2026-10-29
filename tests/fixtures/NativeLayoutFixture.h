#pragma once
#include <iiGUI/WidgetLayout.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include "NativeRepetitionFixture.h"
#include <string>
struct NativeLayoutModel final {
    static inline int live=0;
    NativeLayoutModel() { ++live; }
    ~NativeLayoutModel() { --live; }
    ii::core::Observable<double> gap{10},defaultGap{8},padding{4},left{2},top{3},right{5},bottom{6};
    ii::core::Observable<double> width{220},height{180},minimumWidth{0},minimumHeight{0},flex{0};
    ii::core::Observable<bool> intrinsic{false},measureHidden{false},visible{true},childVisible{true};
    ii::core::Observable<std::string> alignment{"leading"},alignmentName{""};
    ii::core::Observable<std::string> rowAlignment{"bottom"},columnAlignment{"trailing"},overlayAlignment{"bottomTrailing"};
    ii::core::Observable<ii::gui::StackAlignmentAxes> axes{ii::gui::StackAlignmentAxes{ii::gui::StackAlignment::trailing,ii::gui::StackAlignment::leading}};
    ii::core::Observable<double> firstWeight{1},secondWeight{1};
    ii::core::Observable<int> file_revision{0};
    int gap_calls=0;
    double measureGap(double value) { ++gap_calls; return value; }
};
struct NativeLayoutRows final {
    ii::core::Observable<std::vector<std::shared_ptr<NativeLiveItem>>> items{[] {
        std::vector<std::shared_ptr<NativeLiveItem>> values;
        for (int n=0;n<1000;++n) values.push_back(std::make_shared<NativeLiveItem>(std::to_string(n),"Short"));
        return values;
    }()};
    ii::core::Observable<int> width{240},height{180};
    ii::core::Observable<double> rowGap{2},rowPadding{0};
    ii::core::Observable<int> file_revision{0};
};
