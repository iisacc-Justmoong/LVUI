# LVUI 선언과 iiGUI AppCard

`AppCard`의 상태·자연 크기·섹션 배치·그리기와 본문 수명은
`iiGUI/AppCardModel.h`와 `iiGUI/NativeAppCardView.h`가 소유한다. LVUI 컴파일러는
디자인 토큰과 바인딩을 사용해 앱이 iiGUI 위젯을 생성하는 C++을 만든다.
Qt·QML·LVRS 타입이나 팩터리를 호출하지 않는다. 카드 본문은 별도 자식 슬롯이며
스타일·제목 변경은 본문 세대를 교체하지 않는다.

기본 패딩은 18, 섹션 간격은 10, 둥근 반경은 12이고 자동 폭은 최소 280이다.
명시적 크기 0은 자연 크기를 뜻한다. 제목은 17px/600, 부제는 12px/600이며
둘 다 있으면 사이 간격은 4이다. 제목이나 부제가 있을 때만 구분선을 표시한다.
본문 자연 높이의 최소는 1이다. 실린더 모양의 반경은 실제 카드 폭과 높이 중 작은
값의 절반을 사용한다. 색상과 치수·문자열을 검증한 뒤에만 모델을 변경한다.

`ii::gui::NativeAppCard::create`는 루트와 `content_root()`를 제공하고 `set_options`는
기존 본문을 유지하며 입력을 갱신한다. 같은 입력은 다시 적용하지 않는다. 소유 스레드와 위젯 세대의 수명은 호스트
계약을 따른다. 사용자 콜백이 외부에 발생시킨 부수 효과의 롤백을 의미하지 않는다.
비활성화가 입력 취소 콜백을 일으켜 재진입하면 마지막 유효 설정을 유지한다.
콜백 예외는 원인을 보존한 오류로 반환하며 그 전에 적용된 설정을 자동 취소하지 않는다.

기본 선언형 `AppCard`는 이 구현을 직접 생성하며 자식 View를 본문 슬롯에 배치한다.
`title`, `subtitle`, `enabled`, `width`, `height`, `padding`, `spacing`,
`cornerRadius`, `shapeStyle`, `backgroundColor`, `titleColor`, `subtitleColor`와
공통 `visible`·`opacity`·`flex`를 받는다. 관찰 가능한 C++ 멤버는 함께 계산된
후보 설정으로 갱신하며 잘못된 값은 기존 본문을 유지하고 바인딩 오류로 보고한다.
형태는 `.roundRect`/`.shapeRoundRect`, `.cylinder`/`.shapeCylinder` 또는
`lvui::AppCardShape` 값이다. `FRAMEWORK LVRS` 생성 경로는 제거되었다.
전체 플랫폼의 시각·접근성 검증은 진행 중이다.

색상, 패딩·간격·반경과 제목·부제 글자 크기의 기본값은 선택한 LVUI 스타일
프로필에서 생성한다. iiGUI를 직접 사용하는 C++ 코드는 `ii::gui::AppCardOptions`,
`ii::gui::AppCardModel`, `ii::gui::NativeAppCard`를 사용하고 색상을 제공한다.
기존 `lvui::AppCard` 모델과 `lvui::native::NativeAppCard`는 제거했으며
`lvui::AppCardShape`는 기존 뷰모델의 선언 값으로 유지한다.

macOS arm64에서 iiGUI 단독 모델·호스트·설치 소비자, LVUI 코드 생성·호스트 및
설치 패키지 경계 검사를 통과했다. 실제 창, GPU 출력과 다른 운영체제의 실행은
확인하지 않았다.
