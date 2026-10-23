# 네이티브 C++ 위젯과 Core 앱 연결


> 구현 변경: 이 문서의 초기 Cocoa 컨트롤 경로는 [현재 LVRS 자체 위젯 구현](LVRSNative.md)으로 대체되었다.
> Core의 앱·스코프 소유 구조는 유지하며, 현재 레이아웃·입력·그리기는 iiGUI가 직접 처리한다.
> 아래 AppKit 컨트롤 설명과 검증 수치는 이전 구현 시점의 기록이다.

2026-10-22에 런타임 소유 구조를 확장했다. `View` 컴포넌트 소스는 빌드할 때
인스턴스화 가능한 네이티브 C++ 위젯 클래스로 변환한다. LVRS는 외형 값을 제공하고,
앱 런타임과 최종 실행 파일의 진입점·부착 수명은 iiSDKCore가 담당한다.
iiGUI는 Core가 소유한 이벤트 루프를 빌려 네이티브 OS 창과 컨트롤을 연결한다.

```text
LVUI View 소스 → AOT C++ 위젯 클래스
LVRS 외형 값·글꼴 → 네이티브 위젯의 표현
일반 C++ SDK 모듈 + 생성한 위젯 + 앱 구성 함수
    → iiSDKCore의 실행 파일 타깃 + Core main()
    → Core Application / EventLoop / Runtime / 부착 수명
    → iiGUI의 OS 호스트 / Cocoa 위젯 / 이벤트·표시
```

Core는 iiGUI·LVUI·LVRS를 참조하지 않는다. 상위 모듈이 `ApplicationDriver`와
`ApplicationAttachment`의 표준 C++ 콜백을 제공한다. Qt·QML 런타임이나
LVUI 소스·AST·바이트코드 실행기는 배포 UI를 실행하는 데 사용하지 않는다.

## 생성되는 C++ 위젯

`lvui_generated::MainView` 같은 클래스는 기본 생성할 수 있는 `final` C++ 타입이다.
복사하지 않으며 다음 인터페이스를 제공한다.

| API | 역할 |
|---|---|
| `View::options()` | 생성한 창 속성을 제공한다. |
| `view.attach(host, configure)` | 실제 네이티브 위젯을 구성하고 뷰 정체성을 부착한다. |
| `view.widgets()` | 해당 위젯의 약한 네이티브 핸들·ID 조회를 제공한다. |
| `view.detach(host)` | 해당 세대의 네이티브 위젯을 해제하고 소유 C++ 스코프를 반환한다. |

기존 정적 `mount`는 직접 마운트하는 호출자를 위해 유지한다. Core가 앱을 소유하는
새 경로는 위젯 객체를 만들어 `view_attachment`로 부착한다. 생성된 코드는 직접적인
C++ 위젯 생성·속성 설정·메서드 콜백을 호출한다. 런타임은 컴포넌트 이름을 해석하지 않는다.

창이 닫히면 네이티브 입력과 핸들을 먼저 무효화한다. 생성한 C++ 위젯 객체는
가져온 C++ 백엔드를 계속 소유한다. Core가 소유 작업의 접수를 닫고 활성 작업을 정리한 후
역순 해제에서 그 스코프를 반환한다. 창 종료와 C++ 백엔드 소멸을 같은 순간으로 취급하지 않는다.
직접 C++에서 사용하는 경우도 `attach`·`detach`를 짝지어야 하며,
제어하지 않는 외부 작업의 수명을 자동으로 보장하는 것은 아니다.

## Core가 만드는 최종 앱

```cmake
find_package(iiGUI 0.4 CONFIG REQUIRED)
lvui_generate(NAME MainView SOURCE MainView.lvui OUTPUT_VARIABLE generated)
iisdkcore_add_application(MyApp GUI
    SOURCES application.cpp "${generated}"
    LIBRARIES iiGUI::iiGUI
    RESOURCES ${iiGUI_NATIVE_RESOURCES})
target_include_directories(MyApp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
```

```cpp
#include "MainView.generated.h"
#include <iiGUI/Application.h>

ii::core::Result<void> ii::runtime::configure_application(Application& app) {
    using View = lvui_generated::MainView;
    auto host = ii::gui::attach_application(app, View::options());
    if (!host) return std::unexpected(host.error());
    return app.attach(ii::gui::view_attachment(
        "main", *host, std::make_shared<View>()));
}
```

애플리케이션 소스는 구성 함수를 제공하고 Core의 설치 패키지가 `main()`을 제공한다.
Core가 `Application`을 생성하고 구성·실행·종료·오류·종료 값을 관리한다.
`Application::post`는 앱 전체가 공유하는 Core 작업자 풀을 사용한다.
`app.workers()`로 같은 런타임을 빌려 기존 `SerialQueue`·`Concurrent` API와 연결할 수 있다.
`app.event_loop().sender()`로 보낸 타이머·결과는 같은 Core 루프를 구동하는 iiGUI에서 처리한다.
일반 C++로 직접 작성한 위젯도 같은 `attach`·`detach` 인터페이스로 연동할 수 있다.

## LVRS는 외형을 제공한다

현재 네이티브 외형은 LVRS의 검증한 소스 스냅샷을 사용한다.

- 창의 `#141414`에 기본 악센트 2%를 혼합한다. 기본 악센트에서 배경은 `#141619`이다.
- `PushButton`의 기본·hover·pressed·disabled 색상, 높이 22, 좌우 패딩 8, 모서리 반경 8을 적용한다.
- `Label`은 `.title`, `.title2`, `.header`, `.header2`, `.body`, `.description`, `.caption`, `.disabled` 역할을 지원한다.
- 역할에 따른 26·22·17·15·13·12·11 크기와 글꼴 굵기·텍스트 투명도를 적용한다.
- 스택 기본 간격은 8이다. 스타일을 적용한 세로 스택은 LVRS처럼 기본 가운데 정렬이다.
- Pretendard Regular·Medium·SemiBold·Bold와 원문 OFL 고지를 앱 리소스로 부착한다.

실제 입력·접근성·동작은 네이티브 AppKit 컨트롤이 처리한다. 버튼의 표현은
네이티브 `NSButton` 하위 클래스가 LVRS 디자인 값으로 그린다. Qt 버튼이나 QML 객체를 생성하지 않는다.
글꼴 파일은 수정하지 않았으며, 원본·외형 파일 해시를 iiGUI의
`resources/lvrs-native-appearance.json`에 기록했다. 테스트가 소스 변경을 감지한다.

현재 지원 범위는 기존 AOT 컴포넌트 5종이다. 모든 LVRS 컴포넌트·재질·그레인·모션을
완전히 재현하거나 다른 OS의 위젯 마운트를 구현한 결과로 확대하지 않는다.
일반 반응형 필드 바인딩·GPU 코드 생성의 미구현 범위도 유지한다.

## 실제 예제와 검증

`SDK/iiGUI/examples/HelloWorld`는 Core 진입점과 앱 부착을 사용하는 실제 앱이다.
같은 Core 이벤트 루프 정체성, 네이티브 위젯·Pretendard, 타이머·백그라운드 결과,
크기 변경·반복 클릭·정상 종료와 백엔드의 지연 소멸을 검사한다.
설치 소비자 검증은 설치된 Core의 진입점 소스를 실제 최종 바이너리에 컴파일하고,
글꼴 리소스 해시·Qt/파서 부재·소스와 도구가 없는 실행을 검사한다.
기존 [AOT 검증 기록](NativeAOTVerification.md)과 이후 실행 결과를 구분한다.
