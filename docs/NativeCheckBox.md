# LVUI 독립 체크박스

`LVUI/CheckBox.h`는 디자인 값과 선언 계약을 제공하며 선택 상태·배치·체크 곡선과
그리기는 iiGUI가 소유한다. `LVUI/NativeCheckBox.h`는 특정 그래픽 SDK를 직접
참조하지 않는 바인딩 정책이다. 생성된 앱이 iiGUI 모델·호스트와 SVG 디코더를
명시적으로 조합한다. 기본 선택 표시는 LVUI가 보유한 원본 Figma 곡선 자산을
사용하고, 사용자 색상은 iiGUI가 샘플링한 두 선분의 불변 마스크를 사용한다.

```lvui
import vm EditorBackend
Window {
    CheckBox(text: "Remember", checked: vm.remember,
             accessibilityName: "Remember this choice", onToggled: vm.setRemember)
}
```

`text`, `checked`, `enabled`, `accessibilityName`, `shapeStyle`, `boxSize`, `framePadding`,
`boxRadius`, `checkMarkStrokeWidth`, `labelGap`과 상태별 색상·테두리 입력은
상수 또는 반응형 값이다. 색상은 `checkedColor`, `uncheckedColor`,
`disabledCheckedColor`, `disabledUncheckedColor`, `checkColor`,
`checkMarkColorDisabled`, `textColor`, `disabledTextColor`이며, 테두리는
`boxBorderWidthCheckedEnabled`, `boxBorderWidthUncheckedEnabled`,
`boxBorderWidthCheckedDisabled`, `boxBorderWidthUncheckedDisabled`와 대응하는
`boxBorderColor...` 값이다. 기본 지표는 17 × 17 표시기, 0.5 프레임 여백,
3.5 반경, 2 획, 6 레이블 간격 및 13/13 본문 글꼴이다. 크기를 변경하고
반경·획·테두리 너비를 생략하면 그 값은 표시기 크기에 비례한다.

`shapeStyle`은 기본 `.roundRect` 또는 원형 `.cylinder`이며 원본과 같은
`.shapeRoundRect`·`.shapeCylinder`, 숫자 `0`·`1`도 사용할 수 있다. 반응형
백엔드는 `lvui::CheckBoxShape` 또는 유효한 숫자를 전달한다. 원형은 표시기의 절반을
반경으로 사용하고 크기에 따라 자동 조정한다. 잘못된 값은 기존 표시를 보존한다.

둥근 사각형을 기본 강조색으로 선택하면 원본의 체크 곡선과 고정된 파란색 채우기, 비활성 시
회색 채우기와 낮은 불투명도를 사용한다. `checkedColor`가 기본 강조색과 다르면
사용자 색상과 `checkMarkStrokeWidth`로 두 선분을 그린다. 선언형
`useFigmaCheckedAssets` 또는 C++ `use_figma_checked_assets`로 이 자동 선택을
명시적으로 덮어쓸 수 있다. C++에서 이 선택을 `std::nullopt`로 되돌리면 자동
선택이 복원된다. 원형에서는 자산 선택을 켜더라도 사용자 색상과 선분을 사용한다.
스톡 자산을 선택하면 체크 획과 채우기는 자산의 고정
모양·색상을 따르며 사용자 지정 체크 색상과 획 너비는 사용하지 않는다.
미선택 또는 비활성 상태에서는 안쪽 테두리와 상단 음영을 그린다.
`innerShadowSoftColor`·`innerShadowStrongColor` 또는 팔레트의 대응 필드로
음영 색상을 바꿀 수 있다. 체크 자산과 선형 마스크는 68×68 픽셀로 만들어 표시 크기에
맞춰 확대하므로 큰 표시기와 높은 화면 배율에서 원본과 같은 선명도는 보장하지 않는다.
비활성 자산은 원본 SVG 필터를 고정 이미지로 변환한 대체 자산을 사용한다.

클릭, Space/Enter와 접근성 활성화는 선택 상태를 한 번 전환하고
`onToggled(bool)`을 호출한다. 취소하거나 비활성화된 경우 상태를 바꾸지 않는다.
프로그램의 `checked` 변경은 사용자 콜백을 발생시키지 않고, 다른 반응형 입력
갱신은 직전 사용자 선택을 되돌리지 않는다. 잘못된 구성은 마지막 정상 값을
유지한다. 설정 변경은 UI 소유 스레드에서만 허용되며 종료된 뷰는 거부한다.
`group`은 라디오 선택 전용이므로 비어 있지 않은 값은 진단한다.
조합된 체크박스 호스트의 `options()`는 현재 체크 상태를 포함한 구성을 복사해 돌려준다.
앱이 필요한 항목만 변경해 `set_options()`로 적용할 수 있으며 다른 스레드나
폐기된 위젯에서는 조회를 거부한다.
`set_checked(bool)`는 사용자가 마지막 입력 이후 상태를 바꾸었더라도 지정한 값을
명시적으로 적용하며 사용자 클릭 콜백은 발생시키지 않는다. 다른 속성만 갱신할 때
사용자 선택을 보존하는 `set_options()`의 반응형 규칙과 구분한다.

선택 시 기본 체크 자산 전체 또는 사용자 체크 선분을 크기 0.55에서 1로 전환한다.
기본 모션은 360ms OutBack이며 `motionEnabled`, `transitionDuration`과 네이티브
`motion` 값으로 바꿀 수 있다. `transitionDuration`은 0부터 86,400,000까지의 정수
밀리초이며 지정하면 `motion`의 지속 시간을 덮어쓴다. 반동이 표시기 범위를 벗어나는
부분은 그 범위로 잘라낸다. 선택 해제는 체크 표시를 즉시 숨긴다. 배경색은 선택,
포인터 진입·이탈과 누름에 따라 별도의 130ms OutCubic 전환을 사용한다.
상태를 다시 바꾸면 현재 표시값에서 새 목표로 전환하고 접근성 선택값과 콜백은
즉시 반영한다. 모션 비활성·축소 모션 또는 프레임 스케줄러 부재 시 목표값으로
즉시 정착한다. 스톡 자산 위에도 키보드 포커스 표시가 보인다.

체크 곡선은 원본 자산의 경로와 일치한다. 내부 음영은 테두리와 반투명 상단을
사용한 근사이며 원본의 SVG 흐림 필터와 동일하지 않다.
접근성 역할은 이를 지원하는 iiGUI 호스트에 전달되지만 플랫폼별 스크린 리더의
실제 동작 검증과는 별개이다.

`tests/native_owned_toggle_consumer`는 설치 스테이징의 LVUI와 별도로 공급한
iiGUI를 찾아 생성된 체크박스, 스위치와 라디오를 컴파일·링크한다. 창은 열지 않는다.
`LVUI.OwnedCheckBox.Model`, `Codegen`, `Host`는 기본과 사용자 지정 선택 표시,
명시적 자산 전환, 내부 음영, 반응형 색상과 접근성 입력을 검사한다. 프레임 진행,
중간값 재목표, 축소 모션, 포커스, 재진입과 1,025개 체크박스의 외부 관찰자 공존도
창 없이 검사한다. 임베딩 검사는 곡선 경로를 기존 원본 자산과 비교한다.

체크박스 상태·기하·체크 곡선과 모션은 `iiGUI/CheckBoxModel.h`가 담당한다.
LVUI는 디자인 값과 기본 자산 선택 정책을 제공한다. 그리기·입력·접근성과
프레임 진행 및 수명은 `iiGUI/NativeCheckBoxView.h`가 소유한다. 동일한 입력은
재적용하지 않고 퇴역 시 콜백과 주입된 이미지를 해제한다. 바인딩 헤더는
iiSDKCore만으로 사용할 수 있다. macOS arm64에서 독립 모델과 설치 소비자 및
LVUI 생성·호스트·디자인 모델을 창 없이 검사했다.
