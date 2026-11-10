# LVUI 선언과 iiGUI 모달

`iiGUI/ModalModel.h`와 `iiGUI/NativeModalView.h`는 모달의 상태·기하·액션과
팝업 위젯 트리·입력·모션·본문 수명을 소유하며 `iiGUI/ModalPresentation.h`는
제목·설명과 세 액션 슬롯의 표현 입력을 준비한다. LVUI는 `ModalBinding`과
`ModalHostPolicy`를 통해 선언의 색상·크기·아이콘·버튼 스타일과 콜백을 전달한다.
바인딩은 Core 외 SDK를 직접 참조하지 않으며 앱에서 iiGUI의 모델·호스트·자산
디코더와 표현 정책을 주입한다. 구형 `LVUI/Modal.h` 계산 모델은 제거되었다.
이 구현은 Qt, QML, LVRS나 운영체제 기본 컨트롤을 사용하지 않는다. LVUI 기본 폭은
가용 폭에서 좌우 24씩 제외한 뒤 520~980 범위의 선호 폭 760으로 정하고, 창이 더
좁아도 최소 폭을 유지한다. 높이는 안쪽 콘텐츠 측정값과 20 단위 패딩을 합한 값과
최소 높이 210 중 큰 값이다. 프레임은 가운데에서 위로 16 단위 이동한다.

액션 개수 0은 마지막으로 텍스트가 지정된 액션까지 자동으로 표시한다. 개수를 명시한
경우 빈 텍스트에는 `OK`, `Cancel`, `More`를 사용한다. 액션의 활성 상태는 각각
별도로 관리한다. 설명이 없으면 message를 사용한다. 프레임 색상과 배경, 둥근 사각형
또는 원통형 반경도 독립적으로 검증한다. 모델의 문자열 뷰는 구성 변경 전까지만 유효하다.

생성된 앱은 LVUI의 `ModalBinding`을 iiGUI의 `BasicNativeModal`과 조합한다.
iiGUI의 독립 `NativeModal`에는 액션 대체 문구와 표시 스타일을 호출자가 제공한다.
본문 팩터리는
열 때마다 새 활성화를 만들고 닫을 때 해제한다. 제목과 액션은 유지되며 유효한 설정
갱신이 현재 본문을 다시 만들지 않는다. 액션은 소유자 콜백만 실행하고 자동으로 닫지
않는다. 취소 시 닫힘을 먼저 알리며 닫힘 콜백이 다시 열었다면 이전 취소는 전달하지
않는다. 닫힌 상태에서 `set_content()`로 본문 팩터리를 교체할 수 있다.

기본 모션 파라미터는 열기 420ms OutBack, 닫기 150ms InCubic, 초깃값 크기 0.92,
오버슈트 1.45다. 모션을 사용하려면 호스트 프레임 공급자가 필요하다. 헤드리스 검사는
모션을 끈 상태의 모델과 콘텐츠·입력 수명을 검사한다. 실제 플랫폼의 그리기, 모션과
접근성은 아직 확인하지 않았다.

macOS arm64에서 iiGUI 단독 모델·호스트와 설치 소비자, LVUI 생성 코드·호스트와
격리 설치 소비자의 빌드 및 헤드리스 검사를 통과했다. 다른 컴포넌트와 LVUI의 다른
iiGUI 직접 참조는 이번 모달 이전으로 제거되지 않았다.

기본 선언형 `Modal`은 `title`, `description` 또는 `desc`, `message`, `showIcon`,
`buttonCount`, 세 액션의 텍스트·활성 상태, 크기·여백·색상·모션 파라미터와
`onPrimaryClicked`, `onSecondaryClicked`, `onTertiaryClicked`, `onOpened`,
`onClosed`, `onCanceled`를 받는다. 제목, 폭 등 설정은 관측 가능한 값으로 갱신할 수
있다. 잘못된 갱신은 바인딩 오류로 기록하고 이전 값을 유지한다. `shapeStyle`은
`.roundRect` 또는 `.cylinder`다. 이름 아이콘은 LVUI 내장 아이콘 또는
`iconResolver`의 교체 가능한 공급자를 사용하며 `iconSource`는 앱이 공급하는 불변
래스터다. `iconRevision`을 발행해 공급자 내부 변경을 알릴 수 있다. `motion` 공유
모델 입력은 아직 지원하지 않으며 진단한다.

```lvui
import vm EditorBackend
Window {
    Modal(id: "dialog", title: vm.title, motionEnabled: false,
          buttonCount: 2, primaryText: "Save", secondaryText: "Cancel",
          onPrimaryClicked: vm.save) {
        InputField(id: "editor", text: vm.text, onEdited: vm.edited)
    }
}
```

직접 자식은 하나이며 여러 요소는 스택으로 묶는다. 본문 ID와 구독은 개방 단위의
별도 범위에 속하지만 바깥 백엔드 인스턴스는 공유한다. 위젯 핸들로 `open_at`,
`close_popup`, `cancel_popup`을 호출할 수 있다. `open`은 Bool 상수 또는 관측 가능한
Bool을 받는다. `open: true`와 처음에 참인 반응형 값은 해당 세대와 상위 후보가 모두
게시된 뒤 한 번 열린다. 내장 생성 함수와 중첩 본문에서도 같으며 중첩 모달은 각각의
본문을 독립적으로 해제한다. 이후 반응형 값은 열림과 닫힘을 제어하며 관련 없는
갱신은 본문을 교체하지 않는다. 사용자의 닫기 또는 취소가 앱의 Bool을 자동으로
변경하지 않으므로, 앱은 필요한 경우 콜백에서 자신의 상태를 갱신한다.
[열림 바인딩](NativePopupOpen.md)의 오류와 수명 계약을 따른다.

직접 C++ 소비자는 iiGUI의 `ModalModel`과 프레임워크 입력 값을 사용하거나 기존
LVUI 입력 값을 조합할 때 `ModalInputValue<Image>`,
`ModalContentFactoryValue<Builder, Widget>`, `ModalBinding`을 앱에서 조합한다.
macOS arm64에서 상태·액션·배치, 생성 코드, 호스트와 설치 소비자를 검사했다.
실제 창·GPU와 다른 운영체제의 검증은 포함되지 않는다.
