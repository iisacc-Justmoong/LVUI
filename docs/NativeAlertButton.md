# LVUI 독립 경고 버튼

`LVUI/AlertButton.h`는 LVUI 버튼의 입력·텍스트 배치 모델을 조합하면서 경고 버튼의
스타일을 결정한다. 기본 높이는 20, 좌우 패딩은 8, 반경은 4이다. `dialogStyle`을
켜면 좌우 패딩 16과 반경 16을 기본으로 쓰며, 명시적 크기·패딩·반경은 이 값을
대체한다. 일반 대화상자 버튼은 투명 배경과 `#596168` 테두리를 사용하고 호버와
눌림은 각각 흰색 5%, 9%를 더한다. 강조 색상과 텍스트 색상은 앱이 바꿀 수 있다.

```lvui
import vm DialogBackend
Window {
    AlertButton(text: vm.label, tone: vm.tone, dialogStyle: true,
                method: vm.confirm, onClick: vm.clicked)
}
```

기본 선언형 생성은 `text`, `enabled`, `tone`, `dialogStyle`, `width`, `height`,
`horizontalPadding`, `cornerRadius`, `textColor`, `primaryColor`,
`accessibilityName`의 상수와 반응형 입력을 받는다. 접근성 이름이 비어 있으면
현재 문구를 사용한다. `method`는 인자가 없거나
`const lvui::native::AlertButtonEvent&`를 받으며, 이벤트에는 클릭 출처와 종류가
있다. `onPressed`, `onReleased`, `onCanceled`, `onClick`도 등록할 수 있다.
정상 클릭에서는 누름, 해제, `method`, `onClick` 순서로 호출한다. `method`가 화면을
폐기하거나 버튼을 비활성화하면 폐기된 화면의 클릭 콜백은 호출하지 않는다.

`LVUI/NativeAlertButton.h`는 범용 호스트의 입력·포커스·접근성 표면에 이 모델을
연결하고, 그리기와 스타일은 LVUI가 소유한다. 반응형 설정은 전체 후보를 검증한 뒤
반영하므로 유효하지 않은 값은 기존 버튼을 유지하고 정상 값이 들어오면 복구한다.
화면 게시·폐기에 이벤트 수명을 맞추고 재진입한 설정은 최신 갱신을 보존한다.

모델과 어댑터는 Qt, QML, LVRS를 사용하지 않는다. [합성 경고창](NativeAlert.md)은
이 버튼을 조합한다. 실제 운영체제 표시·접근성 검증은 별도 작업이다. 헤드리스 호스트
검사는 창이나 GPU를 실행하지 않는다.
