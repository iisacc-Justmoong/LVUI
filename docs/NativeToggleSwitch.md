# LVUI 독립 스위치

기본 `ToggleSwitch`는 `LVUI/ToggleSwitch.h`에서 상태, 트랙과 손잡이의 배치,
포인터 드래그 및 프레임 기반 모션을 소유한다. `LVUI/NativeToggleSwitch.h`는
앱이 공급하는 iiGUI의 범용 동작 표면과 글꼴·그리기·프레임 접점에 이를 연결한다.
LVUI 모델은 표준 C++이며 Qt, QML 또는 LVRS 구현을 요구하지 않는다.

```lvui
import vm EditorBackend
Window {
    ToggleSwitch(text: "Snap", checked: vm.snapEnabled,
                 accessibilityName: "Snap to grid", onToggled: vm.setSnap)
}
```

`text`, `checked`, `enabled`, `accessibilityName`, `rightToLeft`, `motionEnabled`,
`transitionDuration`, `trackWidth`, `trackHeight`, `trackPadding`,
`knobSize`, `trackCornerRadius`, `knobCornerRadius`, `labelGap`과 문서화된
색상 입력은 상수 또는 반응형으로 전달할 수 있다. 색상 입력은 `onColor`,
`onColorHover`, `onColorPressed`, `offColor`, `offColorHover`, `offColorPressed`,
`disabledTrackColor`, `knobFillColor`, `disabledKnobColor`, `textColor`,
`disabledTextColor`이다. `motion`은 앱의 `lvui::Motion`
값 또는 공유·약한 핸들이며, `transitionDuration`을 함께 지정하면 정수 밀리초로
지속 시간을 덮어쓴다. 기본값은 38 × 22 트랙, 2의 내부 여백, 18의 손잡이,
8의 레이블 간격 및 320ms OutBack이다. 스타일 색상은 LVUI 토큰 스냅샷에서
선택한다. `group`은 스위치가 아닌 라디오 선택의 입력이며 비어 있지 않으면
진단한다.

누르기·Space/Enter·접근성 활성화는 한 번의 상태 전환으로 처리한다. 드래그 중에는
손잡이만 이동하고 놓을 때 상태와 `onToggled(bool)`을 한 번 커밋한다. 포커스 상실
또는 취소는 드래그 전 상태를 유지한다. `rightToLeft`는 손잡이 위치와 드래그 방향을
반전한다. 프로그램의 `checked` 변경은 사용자 콜백을 발생시키지 않으며, 다른 입력의
갱신이 직전 사용자 선택을 되돌리지 않는다. 잘못된 후보는 이전 상태를 유지한다.
호스트 어댑터의 설정 변경은 UI 소유 스레드에서만 허용하며, 다른 스레드 또는 종료된
뷰에서는 LVUI 상태를 바꾸기 전에 오류를 반환한다.

모션은 실제 표시 위치에서 재목표하며 비활성화나 축소 모션 정책에서는 즉시 정착한다.
호스트가 프레임을 제공하지 못하면 상태와 표시를 목표에 즉시 정착한다. 선택 상태는
애니메이션 종료를 기다리지 않고 접근성에 전달한다. 동작 표면의 스위치 역할을 지원하는
iiGUI 호스트가 필요하다. 현재 범위에는 Figma의 그림자, 손잡이 압축·늘어남 및 시스템
축소 모션 설정 자동 연결이 포함되지 않는다. macOS·iOS 소스 매핑과 헤드리스 검사는
실제 기기 또는 스크린 리더 검증을 뜻하지 않는다.

`tests/native_owned_toggle_consumer`는 설치 스테이징의 `LVUI::Native`, AOT 도구와
별도로 공급한 `iiGUI::iiGUI`를 찾아 생성된 스위치를 컴파일·링크하는 소비자이다.
검사는 빌드까지만 수행하며 창을 열지 않는다.
