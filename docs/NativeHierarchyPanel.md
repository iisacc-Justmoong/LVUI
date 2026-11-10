# LVUI 계층 패널

`HierarchyPanel.h`는 계층 패널의 디자인 값을 제공하며 도구 모음·목록·푸터의 영역 배치는 iiGUI가 계산한다.
기본 크기는 200×530이고 도구 모음은 26 높이, 푸터는 기본 숨김이다. 크기와 표시
여부는 `HierarchyPanelConfiguration`으로 바꿀 수 있다. 계산은 Qt, QML, LVRS에
의존하지 않으며 기존 LVUI 계층 목록과 도구 모음의 상태를 변경하지 않는다.

패널이 좁아지면 도구 모음, 푸터 순으로 실제 높이를 제한하고 목록에 남은 높이를
할당한다. 너비 또는 높이가 0인 영역은 표시하지 않는다. 비유한 값, 잘못된 색상과
허용 범위 밖의 크기는 거절한다. `LVUI.OwnedHierarchyPanel.Layout`은 기본 배치,
푸터 표시, 좁은 패널과 유효성 경계를 검사한다.

`iiGUI/NativeHierarchyPanelView.h`는 주입된 도구 모음, 가상 계층 목록과 푸터를
이 배치에 맞춰 조합한다. `LVUI/NativeHierarchyPanel.h`는 Core 외 SDK를 직접
참조하지 않는 값과 `HierarchyPanelBinding`을 제공한다. 입력은 각 구성요소의 LVUI 모델과 교체 가능한 편집기를 받으며, 구성요소의
선택·펼침·드래그·버튼 이벤트를 앱에 전달한다. 크기와 표시 설정을 갱신해도 유지되는
목록 행은 재사용한다. 잘못된 후보가 실패하면 가능한 범위에서 이전 구성과 배치를
복구한다. 이미 외부 소스가 수정된 경우 그 외부 변경은 되돌리지 않는다.

`LVUI.OwnedHierarchyPanel.Host`는 실제 창 없이 가상 행, 세 영역의 이벤트, 크기 변경,
잘못된 후보 복구, 스레드와 수명 경계를 검사한다.

기본 LVUI `Hierarchy` 선언은 이 패널을 직접 생성한다. `controller`와 `editor`는 각각
LVUI의 계층 화면 상태와 교체 가능한 편집 경계이며 `editable: true`에는 편집기가
필요하다. `modelRevision`을 바꿔 외부 소스의 변경을 반영하고, 컨트롤러의 행 높이를
바꿀 때는 별도의 `viewRevision`을 바꿔 문서 수정 없이 목록을 갱신한다. 컨트롤러를 생략하면
패널은 자체 빈 모델을 사용한다.

`countView`는 [계층 목록](NativeOwnedHierarchyList.md)의 개수 View를 그대로
전달하며, 각 가상 행에 별도 인스턴스를 만든다. 선언형 View는 원래 스코프의 값을
읽는다. `countRow: "item"`을 지정하면 `countView` 안에서 `item.entry.count`와
`item.selected`, `item.height` 등 현재 행의 관찰 가능한 값도 읽을 수 있다. C++에서는 `NativeHierarchyPanel::create`의
마지막 `HierarchyCountViewFactory`가 각 행의 관찰 가능한 컨텍스트를 받는다.
팩터리는 패널 생성 시 정하며 교체하려면 새 패널을 구성한다.
`itemDelegate`와 `itemRow`도 내부 목록으로 전달한다. 사용자 행은 기본 행의
선택·펼침·탐색과 가상화 수명을 유지한다. 사용자 행과 `countView`는 함께 지정할 수 없다.

```lvui
import vm LayerModel
Window {
    Hierarchy(controller: vm.controller, editor: vm.editor, editable: true,
              modelRevision: vm.revision, width: vm.width, height: 530,
              toolbarItems: vm.tools, activeToolbarButtonId: vm.activeTool,
              countRow: "item", countView: Label(text: item.entry.count),
              footerVisible: true, onItemActivated: vm.selectItem,
              onToolbarTriggered: vm.activateTool, onFooterClicked: vm.footerAction)
}
```

크기·행 스타일·아이콘 공급자와 리비전·도구 버튼·푸터 표시 및 슬롯은 반응형으로
갱신된다. 목록 활성화는 키, 선택적 ID(없으면 -1), 원본 인덱스를 전달한다. 펼침과
이동은 각각 키와 펼침 값, `HierarchyMovePlan`을 전달한다. 도구 모음 이벤트는
`HierarchyToolbarSelection`, 푸터 이벤트는 슬롯 인덱스와 `ListFooterSlot`을 전달한다.
직접 자식, 이름별 도구 이벤트 및 실제 플랫폼 화면·접근성 검증은
아직 제공하지 않는다. `LVUI.OwnedHierarchyPanel.Codegen`과
`LVUI.OwnedHierarchyPanel.Generated`는 기본 생성 코드와 반응형 조합을 검사한다.
`CountViewGenerated`는 패널 내부 행의 개수 View, 반응형 입력과 가상화 수명을 검사한다.
`CountRowGenerated`는 각 행의 값과 선택·순서 변경을 생성된 View에 전달하는지 검사한다.
`RowDelegateGenerated`는 패널에서 생성한 사용자 행과 포인터 선택을 검사한다.

공통 `iiGUI/PanelLayout.h`는 주입된 설정을 검증하고 좁은 영역도 겹침 없이 나눈다. LVUI의 `PanelLayoutBinding`은 그래픽 SDK를 직접 참조하지 않는다.

패널이 퇴역하면 진행 중인 갱신이 끝난 뒤 자식 핸들·빈 컨트롤러와 입력을 해제한다. 갱신 중 퇴역으로 위젯이 닫히면 해당 갱신은 닫힘 오류로 끝날 수 있다. macOS arm64의 iiGUI 독립 설치 소비자와 LVUI 생성·헤드리스 경로를 검사했다.
