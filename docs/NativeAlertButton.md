# LVUI 독립 경고 버튼

파괴적 톤은 공통 버튼의 상태 정책과 컴파일 시 선택한 빨강 호버·누름 토큰을 사용한다.

iiGUI의 `AlertButtonModel`과 `AlertButtonView`는 경고 버튼의 상태·스타일 계산,
입력·접근성·그리기와 위젯 수명을 담당한다. LVUI 정책은 토큰과 값·콜백을 전달한다.
기본 높이는 20, 좌우 패딩은 8, 반경은 4이다. `dialogStyle`을
켜면 좌우 패딩 16과 반경 16을 기본으로 쓰며, 명시적 크기·패딩·반경은 이 값을
대체한다. 일반 대화상자 버튼은 투명 배경과 `#596168` 테두리를 사용하고 호버와
눌림은 각각 흰색 5%, 9%를 더한다. 기본 강조 색상의 hover·pressed는 Figma에서 읽은
상태별 토큰을 사용한다. 앱이 강조 색상이나 텍스트 색상을 바꿀 수 있으며, 사용자 지정
강조 색상의 hover·pressed는 해당 색상에서 계산한다.
primary 톤의 기본 색상은 정적 또는 반응형 [창 강조색](NativeWindow.md)을 따르며,
명시적 `primaryColor`는 그보다 우선한다. destructive 톤은 독립적인 위험 색을 유지한다.

```lvui
import vm DialogBackend
Window {
    AlertButton(text: vm.label, tone: vm.tone, dialogStyle: true,
                method: vm.confirm, onClick: vm.clicked)
}
```

기본 선언형 생성은 `text`, `enabled`, `motionEnabled`, `enterKeyActivation`, `tone`, `dialogStyle`, `width`, `height`,
`horizontalPadding`, `cornerRadius`, `textColor`, `primaryColor`,
`accessibilityName`의 상수와 반응형 입력을 받는다. 접근성 이름이 비어 있으면
현재 문구를 사용한다. `method`는 인자가 없거나
`const lvui::native::ButtonEventValue<Widget>&`를 받으며, 이벤트에는 클릭 출처와 종류가
있다. `methods`의 중첩·반응형 목록도 등록할 수 있으며 [공통 버튼 메서드 계약](NativeButton.md)을 따른다.
`onPressed`, `onReleased`, `onCanceled`, `onClick`도 등록할 수 있다.
정상 클릭에서는 누름, 해제, `method`, `onClick` 순서로 호출한다. `method`가 화면을
폐기하거나 버튼을 비활성화하면 폐기된 화면의 클릭 콜백은 호출하지 않는다.
누름·놓기·취소는 [공통 버튼 입력 계약](NativeButton.md)을 사용한다.
`enterKeyActivation`은 기본적으로 꺼져 있으며 상수나 반응형 값으로 켤 수 있다.

`LVUI/NativeAlertButton.h`는 기존 타입과 LVUI 토큰을 제공하는 독립 정책·바인딩이다.
앱이 iiGUI 모델과 호스트, 디코더와 위젯 타입을 명시적으로 주입한다. 반응형 설정은 전체 후보를 검증한 뒤
반영하므로 유효하지 않은 값은 기존 버튼을 유지하고 정상 값이 들어오면 복구한다.
같은 설정은 다시 적용하지 않는다.
화면 게시·폐기에 이벤트 수명을 맞추고 재진입한 설정은 최신 갱신을 보존한다.
`motionEnabled`와 C++ `motion_enabled`는 기본적으로 활성화되어 있으며, 색상
및 포커스 전환의 로컬 해제와 전역 축소 모션은 [공통 버튼 계약](NativeButton.md)을 따른다.

`LVUI/AlertButton.h`에는 디자인 입력 값만 남고 구형 `lvui::AlertButton` 모델은
제거되었다. 직접 C++ 소비자는 `ii::gui::AlertButtonModel`과 프레임워크의 입력 값을
사용하거나, 기존 LVUI 값과 `AlertButtonBinding`을 조합한다. 경고창과 모달도 같은
iiGUI 모델로 검증한다. 값과 바인딩은 iiSDKCore 외 SDK를 직접 참조하지 않는다. [합성 경고창](NativeAlert.md)은 이 버튼을
조합한다. macOS arm64의 독립 iiGUI 모델·호스트·설치 소비자와 LVUI 생성·경고창·
모달 헤드리스 검사를 통과했다. 실제 운영체제 표시·접근성 검증은 포함되지 않는다.
