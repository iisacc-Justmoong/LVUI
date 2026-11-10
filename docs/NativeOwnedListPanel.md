# LVUI 독립 일반 목록 패널

`iiGUI/NativeListPanelView.h`는 주입된 [일반 목록 화면](NativeOwnedListView.md),
목록 도구 모음과 푸터를 iiGUI의 공통 영역 배치로 조합한다. LVUI는
`ListPanelBinding`과 설정 값을 제공하며 생성된 앱이 실제 구현을 조합한다. 기본 크기는 170×140이고 도구 모음은 숨김,
푸터는 표시 상태다. `ListPanelInput`은 각 구성요소의 설정을 받으며 크기가 작을 때
도구 모음과 푸터를 남은 범위 안에 배치한다. 잘못된 후보 설정은 가능한 범위에서
이전 화면을 유지한다. 외부 데이터 소스에 이미 게시된 변경은 되돌리지 않는다.

기본 LVUI `List` 선언은 이 패널을 직접 생성한다. `controller`로 앱의
`std::shared_ptr<lvui::ListController>`를 전달하거나, `model`로 `ListSource`를
전달해 별도 화면 컨트롤러를 만들 수 있다. 둘을 함께 지정할 수 없다. 모두 생략하면
패널은 빈 `ListMemory`를 소유한다. `model` 포인터 자체는 생성 시 정해지며, 같은
소스의 데이터 변경은 `modelRevision`으로 알린다. 외부에서 컨트롤러 선택만 바꾸면
관측 가능한 `viewRevision`을 변경해 행을 갱신한다. 이때 문서 리비전과 사용자
활성화 이벤트는 바뀌지 않는다.

```lvui
import vm CollectionModel
Window {
    List(controller: vm.controller, modelRevision: vm.revision,
         width: vm.width, height: 140, toolbarVisible: true,
         toolbarIcon1: "home", footerVisible: true,
         onItemActivated: vm.selectItem, onToolbarIconTriggered: vm.toolAction,
         onFooterButtonTriggered: vm.footerAction)
}
```

`itemDelegate`는 행마다 생성할 단일 View다. 선택적으로 `itemRow`에 유효한 식별자를
문자열로 지정하면 해당 View 안에서 그 이름으로 원본 데이터와 화면 상태를 읽는다.
같은 이름의 외부 변수는 이 View 안에서만 가려진다. `itemRow` 없이도 외부 스코프의
값을 사용하는 View를 지정할 수 있다.

```lvui
List(controller: vm.controller, modelRevision: vm.revision, viewRevision: vm.viewRevision,
     itemRow: "item", itemDelegate: PushButton(text: item.entry.label, onClick: item.trigger))
```

이름 편집이 필요한 목록에는 `editor`와 `editable`을 명시한다. 둘을 제공해도
`entry.editable`이 꺼진 항목은 편집할 수 없다.

```lvui
List(controller: vm.controller, modelRevision: vm.revision,
     editor: vm.editor, editable: vm.canEdit,
     onItemEdited: vm.itemEdited, onItemSubmitted: vm.itemSubmitted,
     onItemRejected: vm.itemRejected)
```

`onItemEdited`는 인덱스, 원본 항목, 필드 이름 `"label"`, 초안을 전달한다.
`onItemSubmitted`는 인덱스, 원본 항목, 확정된 텍스트를 전달하고,
`onItemRejected`는 같은 값에 `ListError`를 더해 전달한다. 편집 결과를 관찰하는
소스는 변경을 게시한 뒤 `modelRevision`을 갱신해야 한다. `editable`이 `true`이거나
동적인 경우 `editor`가 없으면 컴파일 오류이며 활성화 상태와 편집기는 반응형으로 갱신된다.
키보드·포인터와 취소·충돌 계약은 [일반 목록 화면](NativeOwnedListView.md)을 따른다.

행 정렬도 같은 편집기에 명시적으로 연결한다. `onItemMoved`는 이동한 키, 이전
인덱스, 새 인덱스와 그 뒤에 오는 항목의 키를 순서대로 전달한다. 마지막에 놓으면
마지막 인자는 빈 문자열이다. `reorderable`을 `true` 또는 동적으로 지정할 때는
`editor`가 필요하다. 앱이 소유한 원본·편집기와 문서 이력은 앱이 연결해야 한다.

```lvui
List(controller: vm.controller, modelRevision: vm.revision,
     editor: vm.editor, reorderable: vm.canReorder, onItemMoved: vm.itemMoved)
```

`itemRow`는 `itemDelegate`와 함께 사용해야 한다. View와 관찰 컨텍스트의 유지·폐기,
입력 전달, 사용자 View의 가변 행 높이와 C++ 팩터리의 계약은 [일반 목록 화면](NativeOwnedListView.md)을
따른다. C++의 조합된 패널 `create`도 마지막 인자로 같은 `ListRowViewFactoryValue<Builder>`를
받으며 팩터리는 생성 시 고정된다.

모델 리비전, 크기, 행 높이와 색상, 아이콘 공급자, 도구 아이콘, 푸터 슬롯과 표시
상태는 반응형으로 갱신된다. 항목 활성화는 인덱스와 `ListEntry`, 도구 버튼은 인덱스와
아이콘 이름, 푸터는 인덱스와 `ListFooterSlot`을 전달한다. 선택과 스크롤은 데이터
소스를 수정하지 않는다. 지원하지 않는 자식 뷰와 속성은 컴파일 시 진단한다.
`scrollable: false`는 휠·트랙패드·터치와 행 드래그 가장자리 자동 스크롤을 끄며,
현재 오프셋과 프로그래밍에 의한 이동은 유지한다. 동적 값으로 바꿀 수 있다.
기본 추가·삭제 푸터 버튼도 원본 LVRS처럼 명령만 알린다. 실제 삽입·삭제와
Undo/Redo는 앱이 `ListEditor` 또는 자체 문서 명령에 연결해야 한다.

`LVUI.OwnedList.PanelLayout`, `LVUI.OwnedList.PanelHost`, `LVUI.OwnedList.Codegen`과
`LVUI.OwnedList.Generated`는 배치, 가상 행 유지, 반응형 설정, 이벤트와 수명을
헤드리스 환경에서 검사한다. `LVUI.OwnedList.RowDelegateGenerated`는 행별 반응형
입력, 행 명령과 반응형 높이, 외부 선택과 재정렬, 행 이름 없는 View와 폐기를 검사한다.
`LVUI.OwnedList.EditingGenerated`는 선언형 편집 이벤트·충돌과 반응형 설정을 검사한다.
`LVUI.OwnedList.DragGenerated`는 선언형 이동 이벤트와 반응형 비활성화를 검사한다.
더 다양한 행 유형, 실제 플랫폼 화면과 접근성 검증은 남아 있다.

계층 패널과 공유하는 `iiGUI/PanelLayout.h`가 영역을 계산하며, LVUI는 설정과 Core 단독 바인딩을 제공한다.

패널 퇴역 시 진행 중인 갱신이 끝나면 자식 핸들·컨트롤러·입력을 해제한다. 갱신 중 퇴역으로 위젯이 닫히면 닫힘 오류가 반환될 수 있다. macOS arm64의 iiGUI 독립 설치 소비자와 LVUI 패널 생성·헤드리스 검사를 통과했다. 일반 목록 호스트도 iiGUI가 소유한다.
