# ApplicationWindow 사용자 정의 창 프레임

`ApplicationWindow` 또는 `Window`는 생성 시 네이티브 창 프레임을 선택할 수 있다.

```lvui
ApplicationWindow(title: "Example", windowChrome: .custom) {
    VStack { Label(text: "Content") }
}
```

기본값은 `.system`이다. 창이 뷰 트리보다 먼저 생성되므로 `.system` 또는
`.custom`을 정적으로 지정한다. LVUI AOT 결과는 이 값을 iiGUI에 전달하며 LVUI를
런타임에 해석하지 않는다. 타이틀바는 일반 선언형 뷰와 대응하는 뷰모델로 구성한다.

`ii::gui::window_chrome_presentation()`으로 플랫폼별 표시 정책을 구분한다.
macOS에서는 시스템 창 버튼을 위한 공간을 남긴다. Windows와 Linux에서는 LVUI
버튼을 닫기, 최소화, 최대화·복원 동작에 연결한다. iPadOS와 Android에서는
버튼 없이 타이틀바만 표시할 수 있다. 마우스 이동 영역은 뷰모델을 거쳐 iiGUI의
`Host::begin_window_drag()`에 연결한다. 시스템 창 관리 기능과 검증 범위는
따로 있는 iiGUI의 창 프레임 문서를 따른다.

LVUI 소비자와 iiGUI 바이너리는 변경된 `HostOptions` 헤더를 기준으로 함께 다시
빌드한다. 컴파일러 검사는 기본값과 사용자 정의 값, 잘못된 동적 값 및 애플리케이션
장면을 확인하며 실제 운영체제 창을 열지는 않는다.
