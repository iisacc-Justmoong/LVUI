# LVUI 독립 체크박스

기본 `CheckBox`는 `LVUI/CheckBox.h`의 표준 C++ 모델이 선택 상태와 배치, 팔레트,
체크 표시의 형태를 소유한다. `LVUI/NativeCheckBox.h`는 앱이 공급하는 iiGUI의
범용 동작 표면·텍스트 측정·이미지 그리기에 연결한다. 체크 표시는 폰트나 LVRS
자산에 의존하지 않고 LVUI가 두 선분을 샘플링하여 생성한 불변 마스크이다.

```lvui
import vm EditorBackend
Window {
    CheckBox(text: "Remember", checked: vm.remember,
             accessibilityName: "Remember this choice", onToggled: vm.setRemember)
}
```

`text`, `checked`, `enabled`, `accessibilityName`, `boxSize`, `framePadding`,
`boxRadius`, `checkMarkStrokeWidth`, `labelGap`과 상태별 색상·테두리 입력은
상수 또는 반응형 값이다. 색상은 `checkedColor`, `uncheckedColor`,
`disabledCheckedColor`, `disabledUncheckedColor`, `checkColor`,
`checkMarkColorDisabled`, `textColor`, `disabledTextColor`이며, 테두리는
`boxBorderWidthCheckedEnabled`, `boxBorderWidthUncheckedEnabled`,
`boxBorderWidthCheckedDisabled`, `boxBorderWidthUncheckedDisabled`와 대응하는
`boxBorderColor...` 값이다. 기본 지표는 17 × 17 표시기, 0.5 프레임 여백,
3.5 반경, 2 획, 6 레이블 간격 및 13/13 본문 글꼴이다. 크기를 변경하고
반경·획·테두리 너비를 생략하면 그 값은 표시기 크기에 비례한다.

클릭, Space/Enter와 접근성 활성화는 선택 상태를 한 번 전환하고
`onToggled(bool)`을 호출한다. 취소하거나 비활성화된 경우 상태를 바꾸지 않는다.
프로그램의 `checked` 변경은 사용자 콜백을 발생시키지 않고, 다른 반응형 입력
갱신은 직전 사용자 선택을 되돌리지 않는다. 잘못된 구성은 마지막 정상 값을
유지한다. 설정 변경은 UI 소유 스레드에서만 허용되며 종료된 뷰는 거부한다.
`group`은 라디오 선택 전용이므로 비어 있지 않은 값은 진단한다.

현재 표시는 근사한 체크 모양이다. Figma의 정확한 곡선 자산, 내부 그림자,
선택 상태의 색상 보간과 반동 모션, 원형 `shapeStyle`은 아직 구현하지 않았다.
접근성 역할은 이를 지원하는 iiGUI 호스트에 전달되지만 플랫폼별 스크린 리더의
실제 동작 검증과는 별개이다.

`tests/native_owned_toggle_consumer`는 설치 스테이징의 LVUI와 별도로 공급한
iiGUI를 찾아 생성된 체크박스, 스위치와 라디오를 컴파일·링크한다. 창은 열지 않는다.
