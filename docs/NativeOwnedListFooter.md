# LVUI 독립 목록 푸터

`ListFooter`는 LVUI 도구 버튼 세 개로 구성하는 별도 컴포넌트다. 아이콘, 슬롯 배치,
상태와 콜백은 LVUI가 소유하며 Qt, QML과 LVRS 위젯을 생성하지 않는다.

```text
import vm FooterBackend
Window {
    ListFooter(button1: vm.add, interactive: vm.enabled,
               onButtonClicked: vm.trigger)
}
```

기본 슬롯은 파일 추가, 삭제, 설정이며 마지막 설정 슬롯은 메뉴 표시자를 가진다.
표시자는 팝업을 자동으로 열지 않으므로 앱의 콜백에서 메뉴를 열 수 있다. 기본
아이콘 크기는 18, 버튼 높이는 22, 바깥 가로·세로 여백은 2이며 기본 자연 크기는
88×26이다. 숨긴 슬롯은 배치에서 제외하며 좁은 폭에서는 보이는 슬롯을 축소한다.
기본 아이콘은 LVUI의 로컬 SVG 자산으로 제공한다.

`button1`부터 `button3`은 `lvui::ListFooterSlot`, 그 불변 공유 포인터 또는 선택적
값을 받는다. 빈 포인터나 빈 선택 값은 해당 슬롯을 숨긴다. 각 슬롯은 아이콘 이름,
텍스트, 접근성 이름, 활성·표시 여부, 메뉴 표시자와 선택적인 `ListFooterCommand`를
가진다. 앱은 `iconSource`와 `iconRevision`으로 아이콘 공급을 교체할 수 있다.
`onButtonClicked`는 0부터 시작하는 슬롯 번호와 해당 슬롯을 받는다. 슬롯 명령을 먼저
호출하고, 명령 실행 중 푸터가 제거되거나 구성이 바뀌면 오래된 후속 콜백을 보내지
않는다. `interactive: false` 또는 비활성 슬롯은 동작을 보내지 않는다.

C++ 앱은 `NativeListFooter::create()`와 `ListFooterInput`을 사용할 수 있다. 슬롯과
크기, 색상은 반응형으로 바꿀 수 있으며 잘못된 아이콘이나 구성은 기존 푸터를
보존한다. 각 슬롯의 위젯과 콜백 수명은 푸터와 연결되고 생성한 스레드에서 사용한다.

`LVUI.OwnedListFooter.Model`은 기본 배치, 숨김·좁은 영역과 검증을 검사한다.
호스트와 생성 코드 검사는 기본 아이콘, 명령 순서, 반응형 오류 복구, 소유자 제거와
스레드 경계를 헤드리스 트리에서 확인한다. 실제 운영체제 렌더링과 보조 기술 검증은
별도로 필요하다.
