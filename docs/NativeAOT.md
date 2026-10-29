# 실제 AOT 코드 생성과 Cocoa 마운트


> 구현 변경: 초기 Cocoa 컨트롤 경로와 [LVRS 생성 경로](LVRSNative.md)는 과거의 구현이다.
> LVUI는 시각값만 계승하고 자체 컴포넌트 구현으로 전환 중이다. 현재 범위와 남은 의존성은
> [독립 구현 현황](LVUIIndependence.md)을 따른다. Core의 앱·스코프 소유 구조는 유지한다.
> 아래 AppKit 컨트롤 설명과 검증 수치는 이전 구현 시점의 기록이다.
> 현재 동적 스택 입력·상수 폴딩·원자적 변경은 [네이티브 배치](NativeLayout.md)를 따른다.

이후 확장한 [네이티브 C++ 위젯·Core 앱 연결](NativeWidgetRuntime.md)이 현재 앱 구성 경로이다.
아래의 직접 정적 마운트 API도 유지하지만, 새 앱은 생성한 위젯 인스턴스를 Core에 부착한다.

2026-10-22에 구현한 C++23 경로이다. `lvui-compile`은 빌드할 때 컴포넌트 소스를
검사하여 C++ 헤더를 출력한다. 해당 헤더와 애플리케이션을 네이티브 컴파일러로 빌드하면
iiGUI가 Cocoa/AppKit의 실제 위젯을 생성·마운트한다.

```text
HelloWorld.lvui
  → lvui-compile (빌드 호스트의 네이티브 실행 파일)
  → HelloWorld.generated.h (직접적인 위젯 생성과 C++ 콜백)
  → C++23 네이티브 컴파일·링크
  → HelloWorld.app
  → iiGUI Host / NSWindow / NSStackView / NSTextField / NSButton
```

앱은 소스 파일·컴파일러·AST를 읽지 않는다. 생성한 코드는 `lvui::parse`나
`parse_view`를 호출하지 않으며, 실행 시 소스 표현식이나 위젯 명령 목록을 해석하지 않는다.
OS 위젯은 컴파일한 함수가 실행될 때 생성된다.

## 현재 지원하는 입력

| 컴포넌트 | 입력 | Cocoa 대상 |
|---|---|---|
| `ApplicationWindow` | `title`, `width`, `height`, `minimumWidth`, `minimumHeight` | iiGUI의 `NSWindow` |
| `VStack` | `id`, `spacing`, `padding` | 세로 `NSStackView` |
| `HStack` | `id`, `spacing`, `padding` | 가로 `NSStackView` |
| `Label` | `id`, `text`, `fontSize`, `selectable` | `NSTextField` |
| `PushButton` | `id`, `text`, `enabled`, `onClick` 또는 첫 번째 위치 인자 | `NSButton` |

`ApplicationWindow`는 루트이며 자식 한 개를 받는다. 레이아웃은 스택의 `{}`로 합성한다.
라벨·버튼은 자식 뷰를 받지 않는다. `id`는 마운트 내에서 유일하며 생략하면 컴파일러가
`@lvui/N`을 부여한다. 루트 콘텐츠의 너비는 창을 따라가고 높이는 콘텐츠의 자연 크기이다.
간격·패딩은 0∼32767, 글자 크기는 1∼256, 창 크기는 1∼32767 논리 단위이며
초기 크기가 최소 크기보다 작으면 오류이다. 과도한 콘텐츠를 스크롤하는 기능은 아직 없다.

문자열·숫자·불리언 리터럴과 최상위 스칼라 `let`을 지원한다. 타입 표기는 `String`,
`Bool`, `Int`, `Float`, `Double`이며 `Int`는 정확히 표현 가능한 정수인지 검사한다.
해석한 문자열은 UTF-8 바이트를 보존하는 C++ 리터럴로 변환하고 숫자는 정규화한다.
중복 ID·알 수 없는 입력·입력 타입·구현하지 않은 컴포넌트는 소스 행·열을 가진 오류이다.

전체 시각 예약어 107개를 인식하는 것과 위 표의 네이티브 매핑은 다른 범위이다.
나머지 컴포넌트, `var`, C++ 속성의 자동 반응형 바인딩, 지역 선언·import,
커스텀 컴포넌트·어노테이션·GPU 표면 생성은 현재 AOT 컴파일에서 거부한다.
지원하지 않는 반응형 값을 고정 문자열로 바꾸어 의미를 축소하지 않는다.
Win32·Linux 네이티브 위젯 마운트는 아직 구현하지 않았으며 명시적인 `unsupported` 결과를 반환한다.
기존 iiGUI의 Windows·Linux 창 및 GPU 캔버스 구현은 유지한다.

## C++ 백엔드와 동작

```text
import vm HelloBackend
ApplicationWindow(title: "Hello World") {
    VStack(padding: 24) {
        Label(text: "Hello World")
        PushButton(vm.greet, text: "Say Hello")
    }
}
```

`--include HelloBackend.h`로 일반 C++ 선언을 제공한다. 네임스페이스는 LVUI에서
`Demo.Backend`로 표기하며 생성 코드에서는 `::Demo::Backend`가 된다.
백엔드는 기본 생성 가능해야 한다. 생성·소멸·공개 멤버 접근과 인자 없는 메서드 호출의
C++ 타입 적합성은 실제 C++23 컴파일러가 검사한다. LVUI 기반 클래스나 수동 export는 필요 없다.

import는 성공적으로 실행 중인 호스트에 마운트를 시도할 때 스코프당 한 번 생성한다.
생성 코드의 `Scope`가 백엔드를 소유하고, iiGUI의 `WidgetBuilder::retain`이 마운트 수명까지 유지한다.
인스턴스 `attach` 경로에서는 생성된 C++ 위젯도 `Scope`를 소유하여, 창이 닫힌 뒤에도
Core의 소유 작업 정리가 끝나고 `detach`가 호출될 때까지 백엔드 수명을 유지한다.
버튼은 기존 스코프를 캡처한 네이티브 콜백을 연결한다. `vm.greet`는 클릭 시 실행하며,
`vm.greet()`를 콜백 입력에 전달하면 컴파일 오류이다. 버튼 클릭마다 백엔드를 생성하지 않는다.
일반 메서드의 반환값은 버튼 동작에서 버린다.

생성한 `mount(host, configure)`의 선택적 C++ 초기화 함수는 `Scope&`를 받는다.
`backend_0`, `backend_1` 등은 import 선언 순서의 멤버이다. Hello World는 이 함수에서
일반 C++ 콜백을 연결하고 마운트 핸들로 필요한 라벨만 갱신한다.
현재 경로는 임의 C++ 필드의 자동 변경 관찰이나 비동기 작업의 자동 소유권 이전을 구현하지 않는다.
호출자가 백그라운드 작업을 소유하고 Core UI 실행기로 결과를 전달해야 한다.

## CMake 연동

```cmake
find_package(iiGUI 0.4 CONFIG REQUIRED)
lvui_generate(NAME HelloWorld SOURCE HelloWorld.lvui
    OUTPUT_VARIABLE generated INCLUDES HelloBackend.h)
add_executable(HelloWorld MACOSX_BUNDLE main.cpp "${generated}")
target_include_directories(HelloWorld PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(HelloWorld PRIVATE iiGUI::iiGUI)
```

```cpp
#include "HelloWorld.generated.h"
#include <stdexcept>
using View = lvui_generated::HelloWorld;
ii::gui::Host host(View::options());
auto result = host.run({.opened = [&] {
    auto mounted = View::mount(host);
    if (!mounted) throw std::runtime_error(mounted.error().message);
}});
```

`lvui_generate`는 실제 `add_custom_command` 의존성을 사용한다. 소스·도구·실행 스크립트가
바뀌면 다시 생성한다. 생성 C++나 스타일 헤더의 내용이 이전과 같으면 출력의 수정 시각을
유지하여 소비 C++ 객체의 불필요한 재컴파일을 피한다. 내용이 바뀌면 헤더를 교체하며,
실패하면 기존 출력은 덮어쓰지 않는다. `tests/aot_tooling.py`는 일반 뷰·스타일 출력의
내용·수정 시각 유지와 실제 변경 반영을 검사한다.
교차 컴파일에서는 대상 실행 파일을 빌드 호스트에서 실행하지 않도록
`LVUI_HOST_COMPILER=/host/native/lvui-compile`을 명시해야 한다.

명령행 도구는 `--name`, 반복 가능한 `--include`, 입력 파일을 받고 C++를 표준 출력으로 낸다.
종료 값은 성공 0, 소스·의미 오류 1, 파일·사용법·출력 오류 2이다.
네이티브 코드 생성 라이브러리 API는 `LVUI/Compiler.h`의 `compile_native`이다.

## 마운트·해제와 검증

`Host::mount_widgets`는 새 컨트롤을 임시로 구성한 뒤 성공 시 기존 콘텐츠를 교체한다.
실패하면 기존 마운트와 핸들은 유효하다. 해제·교체·창 종료 시 연결을 끊고
기존 핸들을 무효화한 뒤 소유 스코프를 해제한다. 창 종료 알림 전에 이 처리가 끝난다.
실행 중인 콜백은 자신의 복사본을 유지하므로 자신을 해제하는 동작도 안전하게 반환한다.
UI 밖에서의 접근은 `wrong_executor`, 해제 후 접근은 `closed`이다.

Hello World는 `SDK/iiGUI/examples/HelloWorld`에 있다. Core 타이머와 작업자 결과를
네이티브 라벨에 반영하고, 크기 변경과 두 번의 버튼 동작·종료를 검사한다.
`SDK/iiGUI/tests/widgets.mm`는 실제 AppKit 객체 클래스와 Auto Layout, 실패 복구,
오래된 핸들 거부, 소유 객체 수명, 자기 해제와 예외 장벽을 검사한다.
설치 검증 절차와 실행 결과는 [NativeAOTVerification.md](NativeAOTVerification.md)에 기록한다.

이 Hello World는 OS 위젯 자체 그리기를 사용하고 GPU 캔버스를 할당하지 않는다.
iiGUI가 이미 가진 Metal·Vulkan 캔버스는 회귀 테스트로 유지하며,
이번 작업을 LVUI의 GPU 코드·셰이더 자동 생성 구현으로 표현하지 않는다.
