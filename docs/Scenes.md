# 앱 Scene과 렌더 트리 계약

`Scene`은 `Application`이 소유하는 독립적인 창 장면과 그 뷰 세션이다.
앱 드라이버로 실행할 때 각 Scene은 하나의 네이티브 호스트와 하나의 루트 View를
갖는다. 생성된 독립 `build`는 첫 장면의 위젯만 주어진 트리에 구성하며 운영체제
창을 열거나 앱 드라이버를 시작하지 않는다. Scene의 본문에
선언한 상태와 객체는 그 장면을 여는 동안 유지하며, 장면을 닫고 다시 열면 새로
생성한다. 앱 스코프는 열린 장면들에 공유되고 개별 장면을 닫아도 유지된다.

```lvui
Application {
    Scene(id: "main", title: "Workspace") {
        var count: Int = 0
        VStack {
            Label(text: count)
            PushButton(text: "+", onClick: { count = count + 1 })
            PushButton(text: "Preferences", onClick: openScene("preferences"))
        }
    }
    Scene(id: "preferences", title: "Preferences", initiallyOpen: false) {
        PushButton(text: "Close", onClick: closeScene("preferences"))
    }
}
```

Scene은 기존 창 입력 `title`, `width`, `height`, `minimumWidth`, `minimumHeight`,
`accent`, `backgroundColor`, `fontFamily`, `onOpened`, `onClosed`,
`onCloseRequested`를 사용한다. `id`는 선택적인 정적 UTF-8 문자열이다. 각 앱에서
Scene과 기존 Window의 이름은 중복될 수 없다. 첫 장면은 시작 시 열려 있어야 하고,
처음에 닫힌 장면은 이름이 있어야 한다. 동적 열기와 닫기는 기존 창 레지스트리를
사용하며 이벤트가 끝난 뒤 처리된다. 첫 장면만 있는 앱에서는 이 명령을 사용할 수
없다. 동일한 레지스트리를 쓰므로 기존 `openWindow`와 `closeWindow`도 유지된다.
`openScene`과 `closeScene`은 이벤트 콜백으로 전달하거나 `{ ... }` 이벤트 블록의
문장으로 쓸 수 있다. 블록 안의 앞선 상태 변경을 반영해 대상을 평가하고 접수한
대상은 이후 상태가 바뀌어도 변하지 않는다. `openWindow`와 `closeWindow`도 같은
블록에서 사용할 수 있다.
장면 이름이 뒤에 선언되어도 앱 전체의 장면 목록을 확인한 뒤 정적 대상을 검증한다.
`onOpened`는 장면의 위젯 트리가 게시된 뒤, `onClosed`는 게시된 장면이 분리될 때
지역 상태와 객체를 정리하기 전에 각각 한 번 전달한다. 게시되지 않은 생성 실패에는
두 이벤트를 전달하지 않는다. 콜백 실패와 닫기 거부는 기존 창 수명 계약을 따른다.

Scene은 `Application`의 조건 없는 직계 자식이며 Window 안에 중첩하지 않는다.
기존 `Application`의 `Window` 및 `ApplicationWindow` 선언도 계속 사용할 수 있고
새 Scene과 섞을 수 있다. Scene 자체가 한 창을 소유하므로 본문은 Window가 아니라
표시할 루트 View이다. 현행 `.lvui` 소스는 이와 같은 직접 선언을 사용하며 HTML형
태그는 네이티브 컴파일러의 입력이 아니다.

언어는 장면 안의 지역 상태와 현지화도 태그 없이 선언할 수 있다.

```lvui
Application() {
    Scene(title: localized("window.title")) {
        var count: Int = 0,
        VStack() {
            Label() { text: localized("counter.heading") },
            Label() { text: count },
            PushButton() { text: localized("counter.increment"), onClick: { count = count + 1 } },
            PushButton() { text: localized("language.korean"), onClick: setLocale("ko-KR") },
            PushButton() { text: localized("counter.replace"), onClick: {
                setString("counter.heading", localized("counter.replacement"))
            } },
        },
    },
}
```

이 예제의 지역 상태는 해당 Scene에 속한다. 제품의 도메인 상태와 동작은 import한
C++ 뷰모델이 소유하며, 언어의 현지화 함수는 앱이 제공하는 문자열 서비스에 연결된다.
`LVUI.Scene.Codegen`은 이 선언의 상태 할당과 현지화 명령을 생성하는지 검사한다.
`LVUI.Localization.Generated`는 같은 예제를 C++로 컴파일하고 창 없이 버튼을
활성화하여 상태·언어·문자열 변경이 뷰에 반영되는지 검사한다.

생성된 C++ 앱의 `scene_count`와 `scenes`는 선언 순서의 `SceneDescriptor` 배열을
제공하며 기존 Window도 장면으로 나타낸다. `SceneDescriptor`의 `id`는 정적
문자열 뷰이고 `initially_open`은 초기 열기 상태이다. 이름을 생략한 장면의 `id`는
빈 문자열이다. 이 정보는 인스턴스의 현재 열림 상태나 실제 운영체제 창의 생성을
뜻하지 않는다.

각 루트 View는 기존 iiGUI 위젯 트리에 부착되며 입력, 레이아웃, 그리기, 접근성과
장면 스냅샷은 해당 트리와 렌더러의 기존 계약을 따른다. Scene은 별도의 렌더러나
Qt/QML을 도입하지 않는다. 팝업 포털과 오프스크린 장면 그래프의 캡처 경계는
iiGUI의 기존 위젯 장면 계약을 따른다.

실제 다중 창 지원은 호스트에 의존한다. Android와 Web의 기존 보조 창 제한,
iPadOS의 장면 복원 제한, 실제 운영체제·입력기·GPU 검증 범위는 변경되지 않는다.

## 검증 범위

2026-11-06 macOS arm64에서 LVUI 전체 빌드와 관련 자동 검사 47건이 통과했다.
단일·다중 Scene의 상태, 열기·닫기·재생성, 지연 대상 선택, 앱·문서 범위 콜백,
생성 코드의 오류 진단과 기존 Window 호환성을 검사한다. iiGUI의 헤드리스 위젯
트리에서 상태 변경 전후의 그리기 장면과 이전 스냅샷의 불변성도 확인한다.
별도 설치 스테이징을 다른 경로로 옮긴 뒤 Scene 선언을 소비자 앱으로 생성·링크하고
공개 `Scene.h`와 기존 창 소비자를 확인했다. 이 검사는 실제 운영체제 창이나 GPU를
실행하지 않았으며 다른 플랫폼과 전역 설치본의 검증을 뜻하지 않는다.
