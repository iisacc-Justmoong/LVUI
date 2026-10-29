# LVUI 독립 목록 도구 모음

`ListToolbar`는 LVUI가 소유한 세 아이콘 슬롯과 범용 iiGUI 호스트를 사용한다.
아이콘, 크기, 간격, 색상 및 상호작용은 LVUI에서 직접 관리하며 Qt, QML 또는
LVRS 위젯을 생성하지 않는다.

```text
import vm ToolbarBackend
Window {
    ListToolbar(icon1: "home", icon2: vm.secondaryIcon,
                interactive: vm.enabled, onIconClicked: vm.trigger)
}
```

세 슬롯은 각각 `icon1`, `icon2`, `icon3`으로 지정한다. 빈 슬롯은 보이지 않지만
위치를 유지한다. 기본 아이콘 크기는 18, 버튼 크기는 22, 좌우 여백은 8, 상하
여백과 간격은 4이며 배경 모서리 반지름은 8이다. 명시적인 크기가 좁으면 슬롯을
축소해 바깥으로 넘치지 않도록 한다. 비활성 또는 `interactive: false` 상태에서는
아이콘 동작을 보내지 않는다. `onIconClicked`는 0부터 시작하는 슬롯 번호와 현재
아이콘 이름을 전달한다.

앱은 `iconSource`로 `IconSource`를 교체할 수 있다. 공급자의 내용이 변할 때는
공급자 자체 리비전을 갱신하고 관측 가능한 `iconRevision`을 변경한다. 아이콘,
색상, 크기와 활성 상태는 반응형으로 갱신된다. 잘못된 설정이나 아이콘은 기존
도구 모음을 유지하며 호출자에게 오류를 전달한다. 콜백 중 도구 모음이 제거되면
더 이상 입력이나 갱신을 허용하지 않는다. 위젯은 생성한 스레드에서 사용한다.

`LVUI.OwnedListToolbar.Model`은 기본값, 좁은 배치와 잘못된 값을 검사한다.
호스트와 생성 코드 검사는 슬롯 동작, 사용자 콜백, 반응형 실패 복구 및 수명
경계를 헤드리스 트리에서 검사한다. 실제 플랫폼 렌더링과 보조 기술 검증은
별도로 필요하다. [목록 푸터](NativeOwnedListFooter.md)와 [계층 도구 모음](NativeOwnedHierarchyToolbar.md)은
각각 별도 독립 구성 요소다.
