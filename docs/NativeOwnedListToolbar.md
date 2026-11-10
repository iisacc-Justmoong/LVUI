# LVUI 독립 목록 도구 모음

`ListToolbar`의 세 아이콘 슬롯, 배치, 입력·접근성·그리기 및 위젯 수명은
iiGUI가 소유한다. 단독 선언은 생성된 앱 코드에서 iiGUI를 직접 조합하고
LVUI 토큰·아이콘 공급자와 선언형 바인딩을 전달한다. Qt, QML 또는 LVRS
위젯을 생성하지 않는다.

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

독립 C++ 앱은 iiGUI의 `ListToolbarModel`, `ListToolbarView`와 `ListBarInput`에
색상과 아이콘 공급자를 전달할 수 있다. 동일 설정은 다시 적용하지 않지만 진행 중인
이전 공급자 조회보다 우선한다. 조회 중 퇴역하면 남은 아이콘 조회를 중단하고 외부
핸들이 남아 있어도 공급자와 콜백을 해제한다. 배치 중 새 설정이 재진입하면 이전
배치의 남은 슬롯을 중단해 최신 설정을 보존한다.
`LVUI/ListToolbar.h`에는 디자인 입력 값만 남고 구형 `lvui::ListToolbar` 기하
모델은 제거되었다. 직접 C++ 소비자는 iiGUI의 `ListToolbarModel`을 사용하거나
기존 LVUI 값을 조합할 때 `ListToolbarBinding`에 iiGUI 모델·호스트와 디코더를
명시적으로 주입한다. 값과 바인딩은 iiSDKCore 외 SDK를 직접 참조하지 않는다.
목록 패널의 사전 검증도 같은 iiGUI 모델을 사용한다. 목록 패널 자체의 시각 구현과
iiGUI 직접 참조는 아직 남아 있다. macOS arm64에서 iiGUI 독립 모델·호스트·설치
소비자와 LVUI 목록 도구 모음·목록 패널·아이콘 회귀 검사 9건이 통과했다.
기본 내장 아이콘의 픽셀과 사용자 지정 스타일 토큰 전달도 확인했다. 실제 창은
실행하지 않았다.
이후 Python 컴파일러와 새 LVUI 설치 패키지 경계, 생성된 아이콘과 도구 모음도
같은 macOS 헤드리스 환경에서 다시 검사했다.
