# 활성화 소유 비동기 작업과 종료

선택적 `LVUI::Runtime`은 LVUI 활성화와 iiSDKCore `Runtime`·`SerialQueue`·`TaskScope`를
연결한다. 생성된 뷰는 자신의 작업 컨텍스트를 만들고, 소유자가 제거되면 새 작업과 UI
결과 전달을 차단한다. 실행 중 작업·캡처·실행기 슬롯이 모두 퇴역한 뒤 UI 실행 영역에서
백엔드를 파괴한다. 작업마다 스레드를 만들거나 소스 식을 실행하지 않는다.

## 패키지와 생성

이 구성은 `LVUI_BUILD_RUNTIME=ON`으로 빌드하며 비차단 `TaskScope::try_close()`를
제공하는 iiSDKCore 0.20 소스·패키지를 사용한다. 기본 `LVUI::Native`와 컴파일러는 계속
Contracts만 선택할 수 있다. 소비자는 필요한 구성을 명시한다.

```cmake
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Runtime)
target_link_libraries(app PRIVATE LVUI::Runtime)
```

```cpp
ii::runtime::Runtime workers;
lvui::native::WorkRuntime runtime(workers, ui_loop.sender());
auto factories = runtime.factories().with<Model>(
    [](const lvui::native::BackendRequest& request) {
        return Model(lvui::native::WorkRuntime::scope(request));
    });
auto mounted = GeneratedView::mount(host, {}, nullptr, factories);
```

`ui_loop`는 소유 UI 스레드에서 펌프하는 기존 Core 이벤트 루프이다. iiGUI 호스트는
자신의 `event_loop()`를 제공한다. Runtime은 앱이 소유하고 조정자보다 오래 유지한다.
조정자는 약한 UI 전송 끝점을 사용하므로 제거된 이벤트 루프를 역참조하지 않는다.

소유 import가 있는 활성화는 `WorkGroup`을 보유한다. 컨텍스트를 요청한 각 백엔드는
자신의 import 이름에 대응하는 독립 `WorkScope`와 직렬 큐를 받는다. 같은 활성화 안의
서로 다른 백엔드도 병렬로 실행할 수 있다. 소유 import가 없는 레이아웃·
행 스코프에는 불필요한 작업 도메인을 생성하지 않는다. 정책이 없는 기존 뷰는 빈 활성화
자원을 사용한다. 자식이 부모 백엔드를 빌리면 원래 백엔드의 컨텍스트를 계속 사용한다.
`maximum_scopes`와 `stats().scopes`는 실제 생성한 백엔드 작업 도메인의 수를 다룬다.
한 그룹에 속한 모든 도메인이 퇴역하기 전에는 그 그룹의 수신자를 파괴하지 않는다.

## C++ 모델의 작업

`WorkScope::post`는 같은 백엔드의 FIFO `SerialQueue`에 작업을 제출한다. 작업은
`std::stop_token`을 받아 협력적으로 중단할 수 있다. 다른 백엔드는 기존 Runtime의
워커 집합에서 병렬로 실행한다. 제출 결과의 `full`·`stopped` 등은 호출자가 처리한다.
작업과 UI 결과 함수는 이동 전용 캡처도 받을 수 있다. `unique_ptr` 같은 단독 소유 입력을
복사하거나 전역 저장소에 옮겨 두지 않고 해당 작업의 수명에 연결한다.
작업과 결과 함수의 반환 타입은 `void`이다. 비동기 핸들을 반환하고 곧바로 종료한 것으로
오인하지 않도록 그 밖의 반환은 직접 함수 입력에서 거부한다. 이 함수 밖에서 계속하는
작업은 별도의 명시적인 소유 프로토콜이나 앱 작업으로 이전해야 한다.

`WorkScope::deliver`는 UI 결과를 자신의 TaskScope를 통해 전달한다. 워커에서도 사용할
수 있다. 수신자가 유효한 동안만 실행하며, 전달받은 실행기가 잘못된 스레드에서 실행하면
사용자 함수를 호출하기 전에 거부한다. 워커의 원래 예외는 `failure()`에 메시지·원인으로
기록하고 다른 작업과 격리한다. UI 함수·변경 발행의 실패도 같은 경계에 보존한다.
실패 자체도 유효한 UI 수신 경로로 변경을 발행하므로 오류를 읽는 기존 뷰를 갱신할 수 있다.
최종 퇴역 시 원래 예외가 소유한 임의 수신자 캡처는 해제하고 진단의 코드·문자열을 남긴다.
외부에서 복사한 오류 스냅샷은 그 소유권을 유지한다.

```cpp
void Model::start() {
    auto submitted = work->post([this, input = snapshot()](std::stop_token stop) {
        auto output = calculate(input, stop);
        if (stop.stop_requested()) return;
        (void)work->deliver([this, output = std::move(output)] {
            result = output;
        });
    });
    // Product policy handles admission errors here.
}
```

수신 스코프는 대기·실행 작업과 결과 함수의 수명 동안 보존된다. 이것은 임의 C++ 멤버에
동시 읽기·쓰기를 허용하는 잠금이 아니다. 워커는 불변 입력이나 명시적으로 동기화한 상태를
사용하고 UI 상태는 `deliver`에서 갱신한다. `post`는 소유 UI 영역에서 호출한다.

결과 함수 이후 생성된 스코프는 소유 백엔드의 합성 리비전을 한 네이티브 바인딩 배치로
발행한다. 따라서 일반 C++ 공개 값의 변경도 기존 위젯에 반영된다. 이 발행은 파일 저장·
생성 요청을 다시 실행하지 않는다. Core Observable은 기존의 정밀한 변경 추적도 유지한다.

## 생성·제거 순서

1. 생성 정책에서 컨텍스트를 만들고 import를 생성한다. 생성자 안의 작업 제출은 거부한다.
2. 생성된 스코프를 위젯 세대가 보유하고 종료 훅·바인딩 접수를 연결한다.
3. 작업 컨텍스트에 소유자를 연결한 뒤 `configure`·값 초기화·위젯 구성을 수행한다.
4. 제거 시 소유자를 먼저 보존하고 stop을 요청하며 대기 작업·UI 전달을 취소한다.
5. 조정자는 기존 UI 루프의 예약 작업으로 `try_close`를 확인한다. 일반 패널 제거에서
   실행 중 워커를 기다리지 않는다.
6. 캡처와 실행기 슬롯까지 퇴역하면 UI 영역에서 보유한 소유자를 해제한다.

자신의 UI 결과 콜백에서 패널을 제거해도 그 콜백과 캡처가 돌아온 뒤에 파괴한다. 구성
중 작업을 시작한 다음 활성화가 실패하는 경우에도 같은 퇴역 경계를 사용하며 이전 화면을
유지한다. 외부에서 보관한 컨텍스트는 닫힌 뒤 새 작업이나 늦은 결과를 접수하지 않는다.

## 앱의 종료와 역압력

`close()`는 신규 활성화를 차단하고 모든 활성화를 종료 요청한 뒤, 완료 여부를 기다리지
않고 반환한다. `poll()`은 같은 UI 영역에서 명시적으로 정리를 진행한다. UI 예약 접수가
실패하면 `stats().retirement_dispatch`에 원인을 보존하고 수신자를 버리지 않는다. 앱은 UI
루프를 계속 펌프하며 `poll`하거나 최종 종료 경계를 수행해야 한다.

종료 요청은 사용자 stop 콜백을 같은 호출 안에서 실행할 수 있다. 조정자는 먼저 모든
활성 수신자를 보존하고 각 백엔드의 접수를 닫는다. stop 콜백이 다른 대기 작업을 취소하거나
`poll()`을 다시 호출하더라도 일부 도메인만 닫힌 상태에서 공통 수신자를 파괴하지 않는다.
개별 `WorkScope`의 연결·퇴역은 그룹과 조정자가 소유하며 제품은 `post`·`deliver`를 사용한다.

`join()`은 앱의 최종 동기 종료 경계이다. UI 수신을 취소한 뒤 워커와 캡처를 정리한다.
데스크톱에서는 기다릴 수 있으나 워커가 UI 동기 응답을 기다리는 코드는 허용하지 않는다.
브라우저 호스트 등 Core가 대기를 금지한 실행 영역에서는 미완료 시 `would_block`이므로
`close`와 비차단 펌프로 완료한 뒤 소멸해야 한다. 조정자·빌린 Runtime의 소멸 전에 완료
경계를 충족해야 하며, 소멸자는 미완료 작업을 분리된 스레드로 숨겨서 계속 실행하지 않는다.

불투명한 임의 작업을 자동 추적하는 기능은 아니다. 이 컨텍스트나 동일하게 선언한 소유
프로토콜을 통해 작업을 등록해야 한다. 뷰 제거 후에도 계속할 생성·파일 작업은 데이터를
앱 소유 작업으로 명시적으로 이전하고 종료 중 뷰를 캡처하지 않는다.

## 검증 경계

`LVUI.NativeWork.*`는 대기·활성 작업·UI 결과·자기 제거·오류·접수 한계·잘못된 전달 영역·
서로 다른 백엔드의 병렬 실행·재진입 종료를 검사한다. `iiGUI.native_activation_work.*`는
실제 생성 C++에서 일반 값의 비동기 갱신·
파일 상태 비변경·실행 중 패널 제거·자기 결과 제거·작업 시작 후 구성 실패를 검사한다.
Core의 `TaskScopePoll.*`는 비차단 완료·캡처·할당량·cleanup의 공통 경계를 검사한다.
실제 LVRS 창·GPU·다른 플랫폼 실행은 전체 작업의 최종 검증과 구분한다.

### 2026-10-26 검증 기록

현재 macOS arm64 작업 트리에서 다음 결과를 확인했다. 중복되는 검사를 합산하여
독립 기능 수로 표시하지 않는다.

| 검사 | 결과 | 범위 |
|---|---|---|
| LVUI Release CTest | 152/152 통과 | 언어·AOT·관측·설치 소비자와 작업 도메인 8개 검사 |
| AddressSanitizer + UndefinedBehaviorSanitizer | 75/75 통과 | 관련 언어·생성 C++·네이티브 바인딩·작업 런타임 |
| ThreadSanitizer | 8/8 통과 | LVUI 작업 런타임·바인딩, 빌린 Core 라이브러리는 이 구성에서 계측하지 않음 |
| iiGUI 창 없는 통합 검사 | 59/59 통과 | 기존 구성·목록·입력과 비동기 활성화 6개 검사 |
| iiGUI AddressSanitizer + UndefinedBehaviorSanitizer | 59/59 통과 | iiGUI·생성 C++·LVUI Runtime·Core 구현, NativeBindings는 별도 LVUI 검사 |
| Core 런타임 회귀 검사 | 11/11 통과 | 비차단 종료·cleanup·Runtime·Thread·QueryPort·Operation·Concurrent |
| Core 비차단 종료와 cleanup | 6/6 일반·6/6 메모리 검사 통과 | 활성 작업·캡처·슬롯 퇴역과 cleanup 오류 |
| 설치·재배치 소비자 | 3/3 통과 | Native·공개 컴파일러·선택적 Runtime 패키지 |
| LVRS 생성 C++ | 컴파일 통과 | 작업 도메인을 포함한 8개 팩터리, LVRS 앱·테스트 실행은 보류 |

코어 라이브러리와 일반·메모리 검사 Runtime 아카이브의 macOS 최소 버전 메타데이터는
12.0이다. 이는 실제 macOS 12 장치나 다른 플랫폼에서 실행했다는 의미가 아니다.
생성된 위젯 실행 파일에는 Qt 동적 의존성과 LVUI 파서·컴파일러 실행 심볼이 없다.
Dreamscapes QML 81개의 기준선 이후 해시를 보존하며 제품 변환·재설치는 수행하지 않는다.

주요 소스는 [작업 도메인 검사](../tests/native_work.cpp),
[생성 C++ 위젯 검사](../../iiGUI/tests/native_activation_work.cpp),
[설치 소비자](../tests/runtime_consumer/main.cpp),
[Core 비차단 종료 검사](../../iiSDKCore/tests/task_scope_try_close.cpp)이다.
검증 로그와 해시·의존성 기록은 `build/lvui-completion/activation-work-verification-20261026.json`에
둔다. 이 기록은 [전체 LVUI 언어·모듈 완성](Completion.md)을 선언하는 것이 아니다.
