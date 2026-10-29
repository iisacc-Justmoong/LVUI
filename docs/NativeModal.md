# LVUI 독립 모달

`LVUI/Modal.h`는 모달의 크기·정렬·색상·아이콘 파라미터와 제목·설명·액션 정책을
표준 C++ 값으로 소유한다. Qt, QML, LVRS, 호스트 위젯을 사용하지 않는다. 기본 폭은
가용 폭에서 좌우 24씩 제외한 뒤 520~980 범위의 선호 폭 760으로 정하고, 창이 더
좁아도 최소 폭을 유지한다. 높이는 안쪽 콘텐츠 측정값과 20 단위 패딩을 합한 값과
최소 높이 210 중 큰 값이다. 프레임은 가운데에서 위로 16 단위 이동한다.

액션 개수 0은 마지막으로 텍스트가 지정된 액션까지 자동으로 표시한다. 개수를 명시한
경우 빈 텍스트에는 `OK`, `Cancel`, `More`를 사용한다. 액션의 활성 상태는 각각
별도로 관리한다. 설명이 없으면 message를 사용한다. 프레임 색상과 배경, 둥근 사각형
또는 원통형 반경도 독립적으로 검증한다. 모델의 문자열 뷰는 구성 변경 전까지만 유효하다.

`NativeModal`은 이 모델을 범용 iiGUI 팝업과 LVUI 버튼·이미지에 연결한다. 본문 팩터리는
열 때마다 새 활성화를 만들고 닫을 때 해제한다. 제목과 액션은 유지되며 유효한 설정
갱신이 현재 본문을 다시 만들지 않는다. 액션은 소유자 콜백만 실행하고 자동으로 닫지
않는다. 취소 시 닫힘을 먼저 알리며 닫힘 콜백이 다시 열었다면 이전 취소는 전달하지
않는다. 닫힌 상태에서 `set_content()`로 본문 팩터리를 교체할 수 있다.

기본 모션 파라미터는 열기 420ms OutBack, 닫기 150ms InCubic, 초깃값 크기 0.92,
오버슈트 1.45다. 모션을 사용하려면 호스트 프레임 공급자가 필요하다. 헤드리스 검사는
모션을 끈 상태의 모델과 콘텐츠·입력 수명을 검사한다. 실제 플랫폼의 그리기, 모션과
접근성은 아직 확인하지 않았다.

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
`close_popup`, `cancel_popup`을 호출할 수 있다. `open: true`는 최상위 호스트에
마운트한 직후 처음 한 번 열며, 내장 생성 함수와 중첩 본문에서는 초기 개방을 지원하지
않는다. 중첩 모달은 부모 본문이 활성화된 뒤 열 수 있으며 각각의 본문을 독립적으로
해제한다. `open`의 반응형 변경은 아직 지원하지 않는다.
