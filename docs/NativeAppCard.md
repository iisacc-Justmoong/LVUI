# LVUI 독립 카드 구현

`AppCard`의 상태·기본 스타일·자연 크기와 섹션 배치는 LVUI의 C++ 값 모델이
소유한다. `NativeAppCard`는 앱이 공급하는 범용 위젯 호스트의 텍스트 셰이핑,
스택과 그리기 서비스를 사용한다. 두 구현은 Qt·QML·LVRS 타입이나 팩터리를
호출하지 않는다. 카드 본문은 별도 자식 슬롯이며 스타일·제목 변경은 본문 세대를
교체하지 않는다.

기본 패딩은 18, 섹션 간격은 10, 둥근 반경은 12이고 자동 폭은 최소 280이다.
명시적 크기 0은 자연 크기를 뜻한다. 제목은 17px/600, 부제는 12px/600이며
둘 다 있으면 사이 간격은 4이다. 제목이나 부제가 있을 때만 구분선을 표시한다.
본문 자연 높이의 최소는 1이다. 실린더 모양의 반경은 실제 카드 폭과 높이 중 작은
값의 절반을 사용한다. 색상과 치수·문자열을 검증한 뒤에만 모델을 변경한다.

`NativeAppCard::create`는 루트와 `content_root()`를 제공하고 `set_options`는
기존 본문을 유지하며 입력을 갱신한다. 소유 스레드와 위젯 세대의 수명은 호스트
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
