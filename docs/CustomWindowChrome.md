# ApplicationWindow 사용자 정의 창 프레임

`ApplicationWindow`는 기본적으로 시스템 타이틀바와 사용자 정의 타이틀바를
표시하지 않는다. 같은 창 생성 경로를 사용하는 `Window`, `Scene`,
`AppBootstrapWindow`, `AppShell`과 암시적 창에도 이 기본값이 적용된다.
창 콘텐츠는 기존의 기본 좌우 여백을 따른다.

```lvui
ApplicationWindow(title: "Example") {
    VStack { Label(text: "Content") }
}
```

시스템 타이틀바가 필요한 창은 `titleBarVisible: true` 또는
`windowChrome: .system`을 명시한다. `titleBarVisible: false`와
`windowChrome: .custom`은 시스템 타이틀바가 없는 기본 창 프레임을 명시한다.
이 선택은 창을 만들기 전에 결정되는 정적 값이며, 두 입력을 함께 사용할 때
`true`와 `.system`, `false`와 `.custom`만 서로 호환된다.

```lvui
ApplicationWindow(title: "System title", titleBarVisible: true) {
    VStack { Label(text: "Content") }
}
```

앱에서 직접 만든 타이틀바를 표시하려면 `titleBar`에 View를 전달한다.
별도 지정이 없으면 사용자 정의 프레임을 사용하며, 시스템 프레임과 사용자 정의
타이틀바를 함께 지정할 수 없다. `titleBarVisible`은 시스템 타이틀바의 표시를
결정하므로 `false`와 사용자 정의 `titleBar`는 함께 사용할 수 있다. 타이틀바는
일반 선언형 뷰와 대응하는 뷰모델로 구성한다.

```lvui
ApplicationWindow(titleBar: HStack(height: 32) { Label(text: "Title") }) {
    VStack { Label(text: "Content") }
}
```

LVUI AOT 결과는 각 창의 프레임 선택을 iiGUI에 명시적으로 전달하며 LVUI를
런타임에 해석하지 않는다. macOS의 사용자 정의 프레임은 시스템 타이틀 텍스트와
불투명한 타이틀바를 숨기지만 기본 창 버튼은 유지한다. Windows와 Linux에서는
앱이 필요한 창 버튼과 드래그 영역을 제공한다. 모바일과 웹은 각 호스트 정책을
따른다. 기존에 기본 시스템 타이틀바에 의존한 창은 이를 계속 사용하려면 명시적으로
선택해야 한다.

`ii::gui::window_chrome_presentation()`으로 플랫폼별 표시 정책을 구분한다.
macOS에서는 시스템 창 버튼을 위한 공간을 남긴다. Windows와 Linux에서는 LVUI
버튼을 닫기, 최소화, 최대화·복원 동작에 연결한다. iPadOS와 Android에서는
버튼 없이 타이틀바만 표시할 수 있다. 마우스 이동 영역은 뷰모델을 거쳐 iiGUI의
`Host::begin_window_drag()`에 연결한다. 시스템 창 관리 기능과 검증 범위는
따로 있는 iiGUI의 창 프레임 문서를 따른다.

컴파일러와 생성된 코드 검사는 기본값과 명시적 시스템·사용자 정의 타이틀바,
서로 다른 설정을 가진 장면, 충돌 및 잘못된 동적 값을 확인한다. 이 검사는 실제
운영체제 창을 열지 않는다.
