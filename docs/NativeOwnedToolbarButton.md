# LVUI 독립 도구 버튼

`ToolbarButton`은 LVUI가 소유한 버튼 상태, 아이콘·텍스트 배치와 그리기를 범용 iiGUI
입력 표면에 연결한다. Qt, QML과 LVRS 위젯을 사용하지 않는다. 도구 모음과 별도로도
사용할 수 있으며, 이후 도구 모음에서 조합할 수 있는 독립 구성 요소다.

```text
import vm ToolbarBackend
Window {
    ToolbarButton(buttonId: "open", iconName: "home", text: "Open",
                  onClick: vm.open, onTriggered: vm.triggered)
}
```

버튼 ID는 `lvui::ToolbarButtonId`의 정수 또는 비어 있지 않은 문자열이다. 선언형
`onClick`은 인수 없이 호출하고 `onTriggered`는 현재 ID를 전달한다. C++ 앱은
`NativeToolbarButton::create()`에 `ToolbarButtonInput`과 콜백을 전달한다. 두 콜백을
모두 지정한 경우 `clicked`를 먼저 호출하며, 첫 콜백이 버튼을 제거하거나 설정을 바꾸면
오래된 ID를 가진 `triggered`는 전달하지 않는다.

기본 높이는 22, 아이콘은 18, 좌우 여백은 각각 2, 아이콘과 글자 간격은 4, 모서리
반지름은 8이다. 폭을 생략하면 호스트의 텍스트 측정과 아이콘·여백을 사용한다.
명시적으로 좁은 폭에서는 내용이 영역 안에 들어가도록 축소하고 텍스트를 생략 표시한다.
`tone`은 `.primary`, `.default`, `.borderless`, `.destructive`, `.disabled`를 지원하며
기본값은 `.borderless`다. 기본 색상과 포커스 표시는 LVUI가 결정한다.

아이콘은 `iconName`과 선택적인 `iconSource`·`iconRevision`, 또는 앱이 제공하는
`icon` 래스터를 사용한다. 이름과 래스터를 함께 지정할 수 없다. 설정, 표시 여부와
활성 상태는 반응형이며 잘못된 새 설정이나 아이콘을 거절하고 마지막 정상 상태를
유지한다. 버튼은 생성한 스레드에서 사용하고 제거 이후에는 갱신할 수 없다.

`LVUI.OwnedToolbarButton.Model`은 독립 상태와 배치, 입력 검증을 검사한다. 호스트와
생성 코드 검사는 그리기, 클릭·ID, 반응형 오류 복구, 스레드와 콜백 중 제거를
헤드리스 트리에서 확인한다. [계층 도구 모음](NativeOwnedHierarchyToolbar.md)은 이
버튼을 항목으로 조합한다. 직접 자식 등록과 실제 운영체제 렌더링·보조 기술 검증은
별도로 필요하다.
