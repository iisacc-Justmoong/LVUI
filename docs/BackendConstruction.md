# 네이티브 백엔드 생성 입력

소유 import는 활성화마다 C++ 객체를 한 번 만든다. 기본 생성자를 사용할 수 있는 타입은
기존처럼 생성한다. Runtime·실행기·서비스·문서 같은 생성자 인자가 필요한 타입은 앱에서
`lvui::native::BackendFactories`를 전달한다. 모든 멤버 접근과 호출은 계속 정적인 C++
코드이며, 이 생성 함수 등록은 공개 멤버를 별도 등록하거나 export하는 규칙이 아니다.

```cpp
auto runtime = std::make_shared<ii::runtime::Runtime>();
auto factories = lvui::native::BackendFactories{}.with<GenerationModel>(
    [runtime] { return GenerationModel(*runtime); });
auto mounted = GeneratedView::mount(host, {}, nullptr, factories);
```

LVUI 소스는 그대로 `import generation GenerationModel`을 사용한다. 컴파일러가 타입과
생성 위치를 알고 있으므로 `GenerationModel`을 반환하는 C++ 함수를 호출한다. 스크립트의
타입 이름을 실행 중 파싱하거나 임의 생성자 문자열을 해석하지 않는다.

## 공개 C++ 계약

`BackendFactories`는 작은 공유 핸들을 갖는 불변 생성 정책이다.

- `with<T>(factory)`는 새 정책을 반환한다. 기존 활성화가 보유한 정책을 바꾸지 않는다.
- 함수는 정확히 `T`를 반환해야 한다. 인자가 없는 함수 또는 `const BackendRequest&`를
  받는 함수를 사용할 수 있다. 두 형태를 모두 받으면 요청 인자 형태를 선택한다.
- `BackendRequest`의 `type`은 정규화한 import 타입, `binding`은 선언한 지역 이름이다.
  같은 C++ 타입의 두 import에도 이름에 따라 다른 생성자 입력을 줄 수 있다.
- const 타입과 가변 타입은 별도 생성 정책이다. 소유 import를 참조·배열 반환으로 우회하지
  않도록 공개 생성 계약에서도 그러한 타입은 거부한다.
- 요청의 문자열은 해당 함수 호출 중 차용한다. 백엔드가 저장하려면 값을 복사한다.
- 정책에 함수가 없으면 기본 생성자를 검사한다. 기본 생성도 불가능하면 타입 이름이
  포함된 생성 오류를 반환 경계에 전달한다. 임의의 빈 객체로 대체하지 않는다.
- 등록 함수는 최대 4,096개 타입이다. 등록 자체는 백엔드를 생성하지 않는다.

```cpp
auto factories = lvui::native::BackendFactories{}.with<DocumentModel>(
    [services](const lvui::native::BackendRequest& request) {
        return DocumentModel(*services, request.binding == "reference");
    });
```

반환된 C++ 객체는 원래 스코프 저장소에 직접 생성한다. 이동·복사가 불가능한 타입도
지원하며 객체를 힙에 별도 포장하거나 스코프 간 공유하지 않는다. 함수 자체는 이동 전용
자원을 포획할 수 있다. 불변인 것은 생성 정책의 매핑이며, 함수가 명시적으로 참조한 외부
상태까지 고정 스냅샷으로 바꾸지는 않는다. 함수는 소유 UI 실행 영역에서 동기 호출된다.

## 생성된 활성화와 중첩

스코프를 갖는 생성 뷰의 C++ 진입점은 마지막 인자로 정책을 받는다. 기존 인자의 순서는
유지한다.

```cpp
GeneratedView::build(widgets, parent, configure, scope_owner, factories);
GeneratedView::mount(host, configure, scope_owner, factories);
view.attach(host, configure, factories);
```

직접 스코프를 구성하는 소비자는 `std::make_shared<GeneratedView::Scope>(factories)`를
사용한다. 생략하면 기본 생성 정책이다. `configure`는 모든 import 생성 이후의 설정
단계이며, 필수 생성 의존성을 나중에 채우는 용도로 사용하지 않는다.

지역·조건부·반복 스코프는 부모 활성화의 정책을 차용한다. 재활성화할 때마다 해당 정책으로
새 객체를 만든다. 같은 키의 행 유지·일반 속성 갱신·콜백 실행은 객체를 재생성하지 않는다.
기존 백엔드를 자식의 타입 매개변수로 전달하는 동작은 계속 차용이며 팩토리를 호출하지 않는다.

생성 정책은 가장 먼저 생성되는 스코프 저장소가 보유하여 모든 백엔드보다 나중에 파괴한다.
함수만이 소유하는 Runtime·서비스도 백엔드의 소멸자 실행 중 살아 있다. 이후 생성자가
실패하면 앞서 생성된 백엔드를 역순으로 정리하며, iiGUI의 후보 콘텐츠 활성화 실패는 기존
작동 화면을 보존한다. 생성 오류의 원래 C++ 예외를 숨기지 않는다.

## 비동기 작업 경계

이 계약은 실제 iiSDKCore Runtime을 생성자로 전달할 수 있게 한다. 작업마다 전용 스레드나
전역 런타임을 만드는 기능이 아니다. `LVUI::Native` 자체에는 Core Runtime 링크를 강제하지
않으며 Runtime을 사용하는 C++ 백엔드·호스트가 해당 의존성을 선택한다.

생성자 주입만으로 임의 C++ 작업의 캡처와 종료를 추적하지는 않는다. 선택적
[활성화 작업 컨텍스트](ActivationWork.md)는 `BackendRequest.activation`을 통해 전달한다.
백엔드 소유 작업은
기존 Core `TaskScope`·`SerialQueue` 등의 경계에 등록해야 한다. 실행 중 작업을 보존하면서
뷰 종료를 비동기로 완료하는 등록 작업의 계약은 해당 컨텍스트로 연결한다. 임의 네이티브
작업·다른 실행 영역까지 포함한 전체 계약은 [수명 명세](InvocationLifecycle.md)와
[전체 완료 감사](Completion.md)의 검증 대상이다. 실행기가 빌린 참조이면 앱이 해당
참조의 수명을 보장한다. 정책이 공유 자원을 포획하면 스코프가 그 소유권을 유지한다.

## 검증

`LVUI.NativeBackendFactories.*`는 기본 생성, 정확한 반환 타입, 이동 전용 팩토리와
비이동 백엔드, 독립 생성, 정책 교체, 생성 위치별 입력과 오류를 검사한다.
`iiGUI.native_backend_construction.*`는 실제 AOT C++의 저장소 접두 단계·파생 값·차용·
지역 활성화·실패 롤백·자원 파괴 순서와 실제 Core Runtime 주입·워커 실행을 검사한다.
LVRS 생성 팩토리의 오브젝트 컴파일은 실제 창·플랫폼·최종 LVRS 실행과 구분한다.

## 2026-10-26 검증 기록

macOS arm64의 현재 변경에서 다음 결과를 확인했다. 겹치는 검사의 수량은 합산하지 않는다.

| 범위 | 결과 |
|---|---|
| LVUI 전체 Release CTest | 144/144 통과 |
| 관련 언어·네이티브 런타임 ASan·UBSan | 67/67 통과 |
| iiGUI 창 없는 회귀·실제 생성 C++ 연결 | 53/53 통과 |
| iiGUI 구현·생성 C++ ASan·UBSan | 동일 53/53 통과 |
| 설치·경로 이동 독립 소비자 | 코어 실행·컴파일러 소비자 2/2 통과, 새 공개 헤더와 생성 경로 포함 |
| 실제 LVRS 생성 팩토리 | 비이동 백엔드·중첩·차용·생성 입력의 오브젝트 컴파일 통과, 실행하지 않음 |

이동 전용 생성 함수, const 타입 분리, 이름별 입력, 활성화별 독립 객체, 파생 값·중첩 정책
상속, 생성 실패 후 기존 화면 보존과 복구, 자원의 파괴 순서를 검사했다. 앱의 기존 Core
Runtime을 실제 생성자로 전달하고 그 지속 워커에서 작업이 실행되는 것도 확인했다.
이 검사가 불투명한 임의 비동기 캡처의 전체 종료를 입증하지는 않는다.

검사한 생성 위젯 실행 파일은 Qt 동적 의존성과 LVUI 파서·컴파일러 실행 심볼이 없다.
Dreamscapes QML 관측 기준선 81개 파일은 변경하지 않았다. 원시 로그·설치 소비자·소스
해시·실행 경계는 `build/lvui-completion/backend-construction-verification-20261026.json`에
기록한다. 전체 언어·모듈과 실제 창·플랫폼의 완료는 별도의 감사 대상이다.
