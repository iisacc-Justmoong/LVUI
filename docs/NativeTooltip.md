# Tooltip의 네이티브 생성

FRAMEWORK LVRS의 Tooltip은 lvrs::Tooltip::create를 호출하는 C++23 View로 생성된다. 런타임에서 LVUI 원문을 해석하지 않는다. targetId는 같은 구성 스코프의 위젯 ID이며 앞/뒤 선언 모두 연결한다. target은 scoped backend의 typed iiGUI Widget 멤버이다. 두 입력은 함께 사용할 수 없다.

{}의 콘텐츠는 TooltipContentFactory로 한 번 구성하여 닫혀 있을 때도 유지한다. 자식은 활성 뷰의 C++ backend를 공유한다. targetId는 해당 콘텐츠 팩토리의 구성 스코프를 넘어서 찾지 않으며, 다른 도메인의 대상을 연결하려는 요청은 네이티브 API에서 거절한다.

text·accessibilityName, automatic·scrollContent·motionEnabled, 최대 크기·여백·반경·꼬리 크기·surfaceOpacity, 색상, delay·timeout·hideDelay·animationDuration, preferredPlacement·backdrop·motion·availableRect를 전달할 수 있다. anchorX/Y는 대상 로컬 좌표이며 대상이 없을 때 표면 좌표이다. 시간 입력은 숫자 밀리초로 작성하고 유한한 [0,86400000] 범위를 확인한 후 정수 단위로 변환한다.

리액티브 입력은 같은 Tooltip의 options를 갱신한다. 색상·크기·가용 영역 변경으로 콘텐츠를 다시 구성하거나 닫힘 latch를 초기화하지 않는다. 예를 들어 availableRect에는 Observable<ii::gui::Rect>, preferredPlacement에는 Observable<lvrs::TooltipPlacement>를 제공할 수 있다. onOpened/onClosed는 scoped C++ 콜백이다.

원본 Tooltip의 모든 상속 API·초기 open/visible 명령·동적 contentComponent 교체가 구현된 것은 아니다. 지원하지 않는 입력은 AOT 진단으로 거절한다. 원본 대조와 검증 범위는 LVRS docs/native/Tooltip.md에 기록한다. 이 단계의 AOT·소스·설치 소비자는 빌드만 수행하고 앱·테스트 실행은 전체 이식의 최종 일괄 단계에 모은다.
