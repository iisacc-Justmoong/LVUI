# 네이티브 컴파일 지원 범위 조사

예약어 등록은 네이티브 컴포넌트의 구현이나 LVRS의 모든 속성에 대한 호환성을
뜻하지 않는다. `catalog/lvui-keywords.json`의 View를 각각 `Window { 이름() }`으로
생성해 확인하면 일반적인 View를 대상으로 하는 미지원 진단을 조사할 수 있다.
이 검사는 기본 인수가 없는 컴포넌트나 상위 타입이 필요한 컴포넌트를 완성으로
판정하지 않으며 생성된 C++의 빌드와 실행 결과도 대신하지 않는다. `Application`,
`Window`, `ApplicationWindow`, `Scene`은 각자의 올바른 최상위 또는 앱 문맥에서
별도로 검사해야 한다.

## 미구현 공개 타입

다음 타입은 2026-11-08의 단순 AOT 조사에서 직접 사용할 수 없었다.

`InputMethodGuard`, `WindowChromeInteraction`.

`ListItemSelector`는 독립 C++ 모델과 네이티브 UI를 선언형 컴파일에 연결했다.
복합 목록 항목은 `ListItem(type: ...)` 또는 명시적 `ListItemComposite`로 구성할 수
있다. 두 이름은 iiGUI의 복합 항목 구현과 동일한 LVUI 입력 계약을 사용하며,
이전 디자인 시스템의 모든 속성과 호환된다는 뜻은 아니다.
`FocusRing`은 iiGUI의 자체 그리기와 상태 전환을 사용한다. `active`, `width`,
`height`, `radius`, `strokeWidth`, `color`, `motionEnabled`, C++ `motion` 값을 받을 수
있으며 변경 이벤트가 있을 때만 그리기와 필요한 전환 프레임을 요청한다.
`AbstractInputBar`는 iiGUI의 텍스트 입력 위젯에 연결되며 `InputField`의 LVUI
입력·이벤트 계약과 기본 양방향 텍스트 바인딩을 사용한다. 이전 디자인 시스템의
모든 입력 속성을 제공한다는 뜻은 아니다.
독립 `MobileTab`과 `MobileNavigationTab`은 iiGUI의 공통 모바일 탭 그리기·입력
구현을 사용한다. 제공하는 선언 입력과 실제 검증 범위는
[네이티브 모바일 탐색](NativeMobileNavigation.md)에 명시한다.
`WindowNoiseTexture`는 iiGUI의 보관형 이미지 그리기로 연결된다. `width`, `height`,
`color`, `primaryColor`, `noiseStrength`, `blurRadius`, `grainStrength`를 받는다.
기본 색은 LVUI 창과 악센트 토큰이며 노이즈·블러·그레인 강도는 기본 0이므로
필요한 효과를 선언에서 지정한다. 강도는 0–1, 블러는 0–50이다.
`WheelScrollGuard`는 자식 스크롤 뷰의 정적 `targetId`와 `enabled`, `consumeInside`,
`width`, `height`, `onWheelRouted`를 받는다. 휠 입력은 이미 플랫폼에서 논리 픽셀로
정규화되어 있으며, `consumeInside: false`일 때 대상이 경계에 도달하면 바깥 스크롤로
전달된다.
`MobileTabBar`와 `MobileNavigationBar`는 두 앱에서 쓰는 모델·선택·검색·안전 영역을
[네이티브 모바일 탐색](NativeMobileNavigation.md)으로 제공하고 배지·재질·선택 모션도
iiGUI에서 처리한다. `TextEditor`의 일반 텍스트 부분은 별도 설정과 네이티브
View로 제공하며 [지원 범위](NativeTextEditor.md)에 명시한다. 리치 텍스트와 파일
읽기·자동 동기화까지 구현한 것은 아니다.
가드와 창 상호작용은 포커스·입력기·스크롤·OS 창 제어를 요구한다.

## 내부 이름과 기존 fixture

`ActionSlot`, `AdaptiveLayoutHost`, `BodyText`, `CaptionText`, `CardImageContent`,
`CardInformationContent`, `ChannelRow`, `ColorInput`, `ContentText`, `DetailRow`,
`Endpoint`, `Glyph`, `InputSlot`, `Material`, `MoreSlot`, `PreviewSlot`,
`QuantityControl`, `QuantitySlot`, `RadialLayer`, `SegmentsSlot`, `SelectionSlot`,
`SelectorSlot`, `Slot`, `Swatch`, `ToggleSlot`은 카탈로그에서 내부 타입으로
분류되어 있으며 독립 View로 생성할 수 없다. 상당수는 부모의 지역 상태를 참조하는
QML 인라인 슬롯이므로 동일 이름의 독립 위젯으로 추정하지 않는다.

예제 및 테스트 소스 중 `NativeHierarchyCount.lvui`, `NativeHierarchyDelegate.lvui`,
`NativeFunctionValidators.lvui`는 이전 LVRS 타입을 사용하는 미등록 fixture이며
현재 컴파일러에서 실패한다. 각각 현재 LVUI 계층 행 문맥 및 셀 검증 인터페이스로
이전하고 실제 생성 C++를 빌드하는 별도 작업이 필요하다.

`AppBootstrapWindow`와 `AppShell`은 이름 있는 창 초기 상태에 맞춰
[창 호환 경로](NativeWindow.md)를 사용할 수 있다. 기존 Scene과 일반 창, 독립 View의 생성·부착 범위는 [독립 View](NativeEmbeddedViews.md)를
따른다. 이 조사는 Society의 모든 화면·플랫폼이 이전 가능하다는 증거가 아니다.
