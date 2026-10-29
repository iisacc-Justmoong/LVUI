# 재사용 뷰의 타입 지정 이벤트 콜백

재사용 뷰는 네이티브 메서드 참조 또는 C++가 반환한 callable을 입력으로 받는다.
타입은 `() -> Void`, `(String) -> Void`처럼 명시한다. `.lvui` 안에서 처리 함수를
정의하거나 소스 식을 실행하지 않으며, 전달·의존 갱신·호출 코드는 C++23으로 생성한다.

```lvui
import document Model
let Action(run: () -> Void): View = PushButton(run)
let Entry(edit: (String) -> Void = document.record): View = InputField(onEdited: edit)
Window {
    VStack {
        Action(run: document.save)
        Action(run: document.makeHandler(document.amount))
        Entry()
    }
}
```

## 타입과 진단

이벤트 입력의 결과는 `Void`이며 C++ 구현도 `void`를 반환해야 한다. [반환값이 있는 함수](FunctionValues.md)는
표시 계산이나 해당 네이티브 함수 인터페이스에서 사용한다. 이벤트 호출 결과를
무조건 버려 선언 불일치를 숨기지 않는다. 콜백은 최대 16개 인자를 선언할 수 있다.
인자에는 `String`, `Bool`, `Int`, `Float`, `Double`, `Metric`, `Color` 및 정의 환경의
`import class` 청사진 타입을 사용할 수 있다. `Void`·`View`는 이벤트 인자 타입이 아니다.

`String`은 소유 `std::string`, `Int`는 `std::int64_t`, `Float`는 `float`,
`Double`·`Metric`은 `double`, `Color`는 `ii::gui::WidgetColor`에 대응한다.
문자열 이벤트는 네이티브 텍스트만, Bool은 bool만 받는다. Int 이벤트는 정수 payload만
받으며 기존 LVUI 정수 범위를 검사한다. 실수 입력은 유한 값과 선언 타입의 범위를
검사한다. C++ 이벤트 타입은 정확한 payload 타입을 요구하며 차용 참조를 보존한다.
이동 전용 이벤트를 억지로 복사하지 않는다.

다른 재사용 뷰로 다시 전달할 때에는 정규화된 서명이 일치해야 한다. 전달 과정에서
인자 수·타입이 다른 서명으로 바꾸면 LVUI 소스 위치와 함께 거부한다. 최종 메서드의
존재·접근성·호출 가능성·오버로드·반환값과 실제 위젯 이벤트 payload의 호환성은
생성 C++를 검사한다. C++ 헤더를 문자열로 검색하거나 런타임 리플렉션으로 호출하지 않는다.

LVRS의 `Link`·`AlertButton` 등에 있는 `method` 입력은 기존대로 `ButtonEvent`를 제공한다. 명시적으로 인자가 없는
서명은 이 입력의 payload를 사용하지 않으며, 이벤트를 받는 서명은 해당 C++ 청사진을
선언한다. `onClick` 등 일반 이벤트 입력에는 그 입력의 실제 payload를 전달한다.

## 포획·갱신·종료

전달 인자는 호출 위치를 포획하고 생략한 기본값은 정의 위치와 앞선 매개변수를 사용한다.
자식에서 같은 이름의 백엔드를 가져와도 이미 전달한 콜백 수신자는 바뀌지 않는다.
기본값을 앞선 콜백으로 선언하여 같은 참조를 다시 전달할 수도 있다.

메서드 참조를 전달하거나 마운트하는 것만으로 메서드를 실행하지 않는다. C++ callable
팩터리는 [반환 콜백](ReturnedCallbacks.md)의 기존 규칙대로 평가하며, 의존값 변경 시
기존 위젯을 유지하면서 callable을 교체한다. 이동 전용 callable을 복사하지 않는다.

호출 접수는 사용한 뷰 활성화의 경계를 따른다. 자식 제거·루트 종료 이후 호출은
수신자에 접근하지 않는다. 실행 중인 메서드나 callable이 자기 패널을 제거해도 반환 전까지
수신자와 해당 callable 캡처를 보존하며, 반환 후 소유 자원을 퇴역시킨다.

## 현재 범위와 검증

이 문서는 이벤트 콜백 매개변수의 계약이다. 반환 함수 값과 읽기 전용 함수 선언은
[함수 값](FunctionValues.md)을 따른다. LVUI 내부 함수 정의·임의 부분 적용·가변 함수 서명·
전체 템플릿 프로토콜은 포함하지 않는다. 등록한 비동기 작업의 종료는 별도 [작업 경계](ActivationWork.md)를
사용한다. 선언된 콜백이 임의의 외부 비동기 작업을 자동 추적하는 것은 아니다.

반환 팩터리 입력은 [컴포넌트 소유 단일 콜백 값](CallbackValues.md)으로 생성한다.
다중 소비자와 전달·기본값은 같은 값과 현재 함수 버전을 공유한다. 팩터리 의존 리비전이
바뀌면 현재 버전을 교체하되 이미 실행 중인 이전 버전은 반환까지 유지한다.

검사는 [구문·전달·진단](../tests/components.cpp),
[네이티브 타입 적응](../tests/native_callbacks.cpp),
[생성 C++ 위젯](../../iiGUI/tests/native_callable_inputs.cpp)을 사용한다.
LVRS 생성 코드는 컴파일로 확인하며 실제 LVRS 앱·테스트 실행은 전체 목표의 최종 일괄
검증으로 유지한다. 실제 창·GPU·다른 플랫폼의 실행과 창 없는 검사를 구분한다.

### 2026-10-26 검증 기록

현재 macOS arm64에서 다음 결과를 확인했다. 겹치는 검사 수를 독립 기능 수로 합산하지 않는다.

| 검사 | 결과 | 범위 |
|---|---|---|
| LVUI Release CTest | 157/157 통과 | 기존 언어·AOT·관측·설치 소비자와 콜백 서명 검사 |
| AddressSanitizer + UndefinedBehaviorSanitizer | 80/80 통과 | 관련 네이티브 구현·생성 스코프와 계측한 AOT 도구, C++ 거부 검사는 구문 컴파일 |
| iiGUI 창 없는 통합 검사 | 64/64 통과 | 기존 입력·목록·스코프와 콜백 입력 5개 동작 |
| iiGUI AddressSanitizer + UndefinedBehaviorSanitizer | 64/64 통과 | iiGUI·생성 C++·LVUI Runtime·Core 구현, NativeBindings는 별도 LVUI 검사 |
| 마지막 진단 보강 후 재검사 | 일반 15/15·메모리 15/15 통과 | 구성·반환 콜백·비동기 활성화·콜백 입력, 생성 헤더 4개는 보강 전과 바이트 동일 |
| 생성 C++ 타입 검증 | 유효 1개 컴파일·잘못된 6개 거부 | 반환 타입·이벤트 arity·payload 타입·필수 인자·private 멤버·반환 없는 즉시 호출 |
| 설치·재배치 소비자 | 3/3 통과 | 공개 AST·컴파일러·Native·선택적 Runtime, 설치 헤더의 잘못된 콜백 계약도 거부 |
| LVRS 생성 C++ | 10개 팩터리 컴파일 | 인자 없는 method·ButtonEvent·이동 전용 반환 callable 포함, 실행 보류 |

생성된 위젯 실행 파일의 Qt 동적 의존성과 LVUI 파서·컴파일러 실행 심볼을 검사한다.
Dreamscapes의 관측 QML 81개 기준선 해시는 보존하며 제품 변환·재설치는 수행하지 않는다.
재현 로그·소스 해시·패키지·의존성 기록은
`build/lvui-completion/typed-callback-inputs-verification-20261026.json`에 둔다.
실제 LVRS 창·GPU·모든 플랫폼과 [전체 언어·모듈 완성](Completion.md)은 계속 별도 완료 조건이다.
