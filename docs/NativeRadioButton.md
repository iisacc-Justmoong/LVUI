# LVUI 독립 라디오 버튼

기본 `RadioButton`의 선택 상태, 그룹, 원형 표시와 점 모션은 LVUI가 소유한다.
`LVUI/RadioButton.h`는 표준 C++ 표시 모델이고, `LVUI/RadioGroup.h`는 약한 참여자
인터페이스를 통해 그룹을 관리한다. `LVUI/NativeRadioButton.h`는 앱이 공급하는
iiGUI의 범용 동작 표면·접근성·글꼴·프레임 접점에 모델을 연결한다.

```lvui
import vm EditorBackend
Window {
    VStack {
        RadioButton(text: "Original", group: "source", checked: vm.original,
                    onToggled: vm.chooseOriginal)
        RadioButton(text: "Edited", group: "source", checked: vm.edited,
                    onToggled: vm.chooseEdited)
    }
}
```

비어 있지 않은 같은 `group`에는 선택된 참여자가 최대 하나이다. 사용자 활성화는
현재 선택을 해제하지 않으며 새로운 항목을 선택했을 때만 해당 항목의
`onToggled(true)`를 호출한다. 그룹에서 밀려난 항목과 프로그램의 `checked` 변경에는
사용자 콜백을 보내지 않는다. 빈 그룹은 다른 버튼과 독립적이다. 하나의 생성된 뷰
활성화는 그룹 소유자를 가지며 중첩 스코프, 조건부 자식과 키별 반복 행이 이를 함께
사용한다. 별도 활성화는 기본적으로 별도 소유자를 가진다. C++ 앱은 생성된 스코프의
`radio_groups`에 공유 `lvui::RadioGroups`를 명시적으로 공급할 수도 있다.

후보 뷰는 그룹에 약하게 등록하지만 실제 선택에 참여하는 시점은 해당 세대와 모든
상위 후보가 공개된 이후이다. 생성이나 커밋이 실패한 후보는 기존 선택을 바꾸지 않는다.
복수 항목이 초기에 선택된 같은 그룹에서는 공개 순서상 마지막으로 참여한 항목이
선택된다. 퇴역한 항목은 제거된다. 그룹은 모든 관련 모델의 선택을 먼저 변경하고
호스트에 알리며, 알림 중 재진입이 발생하면 최종 상태를 다시 정리한다. 정리 횟수는
제한하며 선택·설정 호출에서 정리가 끝나지 않으면 `busy`를 반환한다. 공개 콜백은
반환 경로가 없으므로 그 시점의 정리 실패는 호출자에게 전파되지 않는다.
그룹 알림이 설정 갱신 중 다른 항목을 선택하더라도 텍스트와 접근성 이름을 포함한
최신 구성을 호스트에 전달한다. 그룹 소유자와 참여자는 UI 소유 스레드에서 사용한다.

`text`, `accessibilityName`, `checked`, `enabled`, `group`, `indicatorSize`,
`dotSize`, `labelGap`, `motionEnabled`, `motion`, `transitionDuration` 및 색상은 상수
또는 반응형 값이다. `state`는 `checked`, `available`은 `enabled`의 대체 입력이며
각 쌍을 동시에 지정하면 진단한다. 색상 입력은 `onColor`, `offColor`,
`onColorDisabled`, `offColorDisabled`, `dotColor`, `dotColorDisabled`, `textColor`,
`disabledTextColor`이다. 기본 표시기는 18 × 18, 점은 8 × 8, 텍스트 간격은 8,
본문은 13/13 Medium이다. 기본 점 전환은 180ms OutBack이며 앱이 모션 정책을
전달하거나 비활성화할 수 있다. 프레임을 공급하지 못하는 호스트에서는 즉시 정착한다.
잘못된 후보는 이전 구성을 유지하고, 다른 반응형 입력의 변경은 사용자 선택을 되돌리지
않는다. 설정 변경은 UI 소유 스레드에서만 허용되고 종료된 뷰는 거부한다.

포인터·터치·Space/Enter 및 접근성 활성화를 범용 표면에서 받는다. 화살표 키로
그룹 내 포커스를 이동하는 정책과 표시기 색상 보간은 아직 구현하지 않았다. 플랫폼별
실제 화면과 스크린 리더 동작은 헤드리스 검사와 별도로 검증해야 한다.

`tests/native_owned_toggle_consumer`는 별도 스테이징한 LVUI와 iiGUI만 찾아 스위치,
체크박스, 라디오의 생성 C++를 컴파일·링크한다. 검사는 창을 열지 않는다.
