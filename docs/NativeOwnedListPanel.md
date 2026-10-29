# LVUI 독립 일반 목록 패널

`NativeListPanel`은 [일반 목록 화면](NativeOwnedListView.md), 목록 도구 모음과 푸터를
LVUI가 소유한 영역 배치로 조합한다. 기본 크기는 170×140이고 도구 모음은 숨김,
푸터는 표시 상태다. `ListPanelInput`은 각 구성요소의 설정을 받으며 크기가 작을 때
도구 모음과 푸터를 남은 범위 안에 배치한다. 잘못된 후보 설정은 가능한 범위에서
이전 화면을 유지한다. 외부 데이터 소스에 이미 게시된 변경은 되돌리지 않는다.

기본 LVUI `List` 선언은 이 패널을 직접 생성한다. `controller`로 앱의
`std::shared_ptr<lvui::ListController>`를 전달하거나, `model`로 `ListSource`를
전달해 별도 화면 컨트롤러를 만들 수 있다. 둘을 함께 지정할 수 없다. 모두 생략하면
패널은 빈 `ListMemory`를 소유한다. `model` 포인터 자체는 생성 시 정해지며, 같은
소스의 데이터 변경은 `modelRevision`으로 알린다.

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

모델 리비전, 크기, 행 높이와 색상, 아이콘 공급자, 도구 아이콘, 푸터 슬롯과 표시
상태는 반응형으로 갱신된다. 항목 활성화는 인덱스와 `ListEntry`, 도구 버튼은 인덱스와
아이콘 이름, 푸터는 인덱스와 `ListFooterSlot`을 전달한다. 선택과 스크롤은 데이터
소스를 수정하지 않는다. 지원하지 않는 자식 뷰와 편집 속성은 컴파일 시 진단한다.

`LVUI.OwnedList.PanelLayout`, `LVUI.OwnedList.PanelHost`, `LVUI.OwnedList.Codegen`과
`LVUI.OwnedList.Generated`는 배치, 가상 행 유지, 반응형 설정, 이벤트와 수명을
헤드리스 환경에서 검사한다. 사용자 행 대리자, 삽입·삭제·이름 변경 UI, 더 다양한 행
유형, 실제 플랫폼 화면과 접근성 검증은 남아 있다.
