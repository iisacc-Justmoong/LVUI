# LVUI 네이티브 링크와 탐색 경계

기본 C++ AOT 경로의 `Link`는 LVUI가 작성한 `Link` 상태·배치 모델과
`NativeLink` 어댑터를 사용한다. 호스트가 제공하는 범용 동작 표면은 포인터·
키보드 입력과 접근성 링크 역할을 전달하고, 글자·포커스 표시와 상태별 색상은
LVUI가 계산한다. 생성 코드와 이 경로는 LVRS 타입이나 팩터리를 호출하지 않는다.
자식 View는 같은 스코프와 위젯 세대에 속하며, 자식이 있으면 텍스트 대체 표시를
숨긴다. 글자 밑줄은 호스트 텍스트 셰이핑에 전달한다.

```lvui
import vm Backend
Window {
    Link(text: vm.text, to: vm.href, navigator: vm.navigator,
         params: vm.params, replace: vm.replace, underline: true,
         onNavigated: vm.navigated) {
        Label(text: "Open report")
    }
}
```

## 교체 가능한 탐색 인터페이스

앱의 탐색기는 `lvui::NavigationTarget`을 구현하여 `NavigationRequest`를 받는다.
요청은 경로, 타입을 보존하는 `ii::core::Value` 매개변수, 선택적인
`NavigationPage` 대상과 대체 여부를 포함한다. 페이지 대상이 있으면 앱의 탐색기는
경로보다 그 대상을 우선한다. `NavigationPage`는 앱이 파생 타입으로 정체성과
팩터리를 정의할 수 있는 호스트 비의존 인터페이스이다. 탐색기는 수락 여부 또는
오류를 반환하며, 같은 링크의 중첩 탐색은 `busy` 오류로 거절한다.

탐색 대상은 살아 있는 명시적 `router`, `navigator`, 생성 범위의
`NavigationContext` 순으로 선택한다. 세 대상이 없거나 경로와 페이지가 모두 없으면
탐색을 수행하지 않는다. 기본 컨텍스트는 생성된 범위의 `navigation_context`로
앱에서 설정할 수 있고 대상은 약한 참조이므로 앱이 실제 탐색기의 수명을 소유한다.
다른 중첩 View는 부모 범위의 컨텍스트를 공유한다. 범위 구성과 위젯 갱신은 호스트의
소유 스레드에서 수행한다. LVUI의 [독립 페이지 라우터](NativePageRouter.md)는
자신을 해당 기본 컨텍스트에 게시하고 앱이 제공하는 페이지 빌더를 연결한다.

경로·텍스트는 최대 64 KiB의 NUL 없는 UTF-8이고, 매개변수는 최대 4096개 노드,
깊이 64, 문자열·바이트 총합 1 MiB를 허용한다. 유효하지 않은 반응형 입력은 기존
설정을 유지하고 바인딩 오류로 보고한다. 앱에서 직접 `trigger_navigation()`을
호출할 때는 위젯의 시각적 비활성화와 별도로 탐색할 수 있다. 사용자 활성화는 호스트의
비활성화·포커스 경계를 따른다.

일반 C++ 멤버를 앱이 외부에서 바꾼 경우 생성된 범위의 `publish_native_changes()`로
알린다. 이 동기 게시 중 갱신이 거절되면 예외가 전달되며 바인딩 오류 목록에서도
원인을 확인할 수 있다. 관찰 가능한 값은 해당 값의 변경을 자동으로 구독한다.

## 생성 입력과 이벤트

지원 입력은 `text`, `href` 또는 별칭 `to`, `params`, `router`, `navigator`,
`targetComponent`, `replace`, `enabled`, `underline`, `tone`, `linkColor`,
`hoverColor`, `pressedColor`, `disabledColor`, `horizontalPadding`, `verticalPadding`,
`minimumHeight`, `cornerRadius`, `width`, `height`, `showFocusRing`,
`accessibilityName`, `method`, `methods`, `onClick`, `onPressed`, `onReleased`,
`onCanceled`, `onNavigated`, `onNavigationFailed` 및 공통 가시성·불투명도·flex이다.
`href`와 `to`를 동시에 지정하면 오류이다. `params`와 탐색 대상은 C++ 백엔드의
타입 있는 값으로 공급한다. `tone: .disabled`는 입력을 차단하고, 다른 tone에서도
링크 배경은 투명하다. 색상과 크기 및 경로는 반응형으로 갱신된다.

`lvui::LinkEvent`와 `LinkMethod`는 호스트 헤더 없이 `LVUI/Link.h`에서 사용할 수 있다.
메서드는 `lvui::LinkEvent` 인자 또는 인자 없는 호출을 지원하고 목록은 최대 256개이다.
사용자 활성화에서는 탐색 결과 알림 다음에 단일 메서드, 메서드 목록, 클릭 콜백을
순서대로 호출한다. 콜백이 링크 세대를 폐기하면 나머지 콜백은 실행하지 않는다.
탐색기와 결과 알림의 예외는 오류와 원인으로 전달하며 커밋된 탐색을 자동 재시도하지
않는다. 상태 전환 콜백에서 링크 설정이 재진입하면 마지막 유효 설정이 유지된다.

LVUI의 상속 모션·플랫폼별 포커스와 모든 버튼 입력 변형, 윈도우 간 기본 탐색기 및
실제 플랫폼 접근성은 아직 남아 있다. 실제 창·GPU·운영체제 및 기기 실행과 코드·
호스트 상태 검사는 별도 증거이다.

## 레거시 경로

이전 `FRAMEWORK LVRS` 경로는 LVRS `Link`와 탐색 타입을 생성했다. 이 경로는
제거되었으며, 자체 구현에 없는 기능의 이전과 플랫폼 검증은 계속 진행 중이다.
