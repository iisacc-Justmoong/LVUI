<a id="declarative-views-and-instance-bound-actions"></a>

# 선언적 뷰와 인스턴스 연결 동작

상태: 2026-10-20에 채택한 설계이다. 예제는 제공된 스코프 소유권 제안을 따른다.
전체 문법은 아직 초안이며 현재 LVUI 0.1 파서는 이 구문을 허용하지 않는다.

<a id="compilation-and-mounting"></a>

## 컴파일과 마운트

LVUI는 [AOT로 네이티브 코드로 컴파일](CompilationModel.md)한다.
iiSDK 런타임은 컴파일된 UI를 iiGUI 그리기 대상으로 마운트한다.
컴포넌트 선언·표현식·어노테이션은 컴파일러 입력이며,
애플리케이션 실행에는 컴파일된 네이티브 동작을 사용한다.
채택한 실행 경로는 소스를 해석하거나 언어 VM을 요구하지 않는다.

<a id="working-notation"></a>

## 설계 표기

LVUI는 컴포넌트 이름, 괄호로 감싼 입력,
자식 뷰·속성 선언을 담은 중괄호 본문으로 뷰를 기술한다.

```text
import document DocumentBackend

ApplicationWindow(accentColor: .blue) {
    VStack() {
        AppControlBar(backend: document)
        AppBodyView(backend: document)
        Footer(status: document.status)

        PushButton(document.save) {
            text: "Save"
            type: .borderless
            enabled: document.canSave
        }
    }
}
```

import 형식은 `import bindingName CppObjectName`이다.
`document`는 뷰에서 사용할 이름이며 `DocumentBackend`는 C++ 백엔드 타입이다.
이 문서의 소유 뷰 스코프를 활성화하면 인스턴스를 생성하고 `document`에 연결한다.
`AppControlBar`와 `AppBodyView`는 같은 인스턴스를 빌려 사용한다.

가져온 객체의 공개 멤버는 `@export` 없이 접근할 수 있다.
private·protected 멤버는 C++ 접근 제한을 유지한다.

import 피연산자 순서는 확정하였다.
소문자 `accentColor`와 점 접두 값 표기는 아직 설계용 표기이며,
전체 파싱·타입 해석 규칙은 추가 명세가 필요하다.

<a id="constants-variables-and-statement-endings"></a>

## 상수·변수와 문장 종결

`let`은 상수, `var`는 변경 가능한 변수를 선언한다.
타입·초기값을 지정하는 선언은 콜론 뒤에 타입, 등호 뒤에 초기값을 둔다.

```text
let constant: Int = 6
var variable: Int = 6
```

LVUI는 세미콜론 종결자를 사용하지 않는다.
변수 선언·import·컴포넌트 구문에 모두 적용한다.
전체 여러 줄 구문과 줄 경계 규칙은 미정이다.

채택한 선언 의미와 ViewModel 상태와의 관계는
[VariableDeclarations.md](VariableDeclarations.md)를 참조한다.

선언 앞에 `@Observable` 또는 `@Static`을 붙일 수 있다.

```text
@Observable var value: Int = 6
@Static let initialValue: Int = 6
```

값은 [기본적으로 반응형](ReactiveState.md)이다.
`@Observable`은 관찰을 명시할 수 있지만 변경 추적·바인딩의 필수 조건은 아니다.
`@Static`은 설정된 수명 동안 초기화된 값의 변경을 금지한다.
채택한 의미와 추가 설계가 필요한 선언 대상 규칙은
[DeclarationAnnotations.md](DeclarationAnnotations.md)에 기술한다.

<a id="method-references-and-calls"></a>

## 메서드 참조와 호출

`vm.increment`는 기존 VM에 연결된 메서드를 참조한다.
`vm.increment()`는 해당 표현식을 평가할 때 명시적으로 호출한다.
메서드 호출 결과는 자체 타입이 있는 값이며 지연 호출로 암묵적으로 변환하지 않는다.

컴포넌트 인터페이스는 입력이 callable을 받는지 선언하고,
구현은 해당 콜백의 호출 시점을 결정한다.
LVUI 언어는 멤버 표현식이나 모든 첫 번째 인자에 클릭 의미를 부여하지 않는다.
타입 검사·반환되는 callable·사용자 정의 컴포넌트·수신 객체 수명은
[MethodReferences.md](MethodReferences.md)를 참조한다.

<a id="meaning-of-declarations"></a>

## 선언의 의미

| 형식 | 채택한 의미 |
|---|---|
| `let constant: Int = 6` | `6`으로 초기화한 `Int` 상수를 선언하며 재대입을 허용하지 않는다. |
| `var variable: Int = 6` | `6`으로 초기화한 변경 가능한 `Int` 변수를 선언한다. |
| `vm.increment` | 기존 VM에 연결된 메서드를 호출하지 않고 참조한다. |
| `vm.increment()` | 표현식을 평가할 때 메서드를 명시적으로 호출한다. |
| `import document DocumentBackend` | 이름 `document`를 새로 생성한 스코프 소유 `DocumentBackend` 인스턴스에 연결한다. |
| `ApplicationWindow(accentColor: .blue) { ... }` | 애플리케이션 창과 강조색 구성을 선언한다. |
| `VStack() { ... }` | 자식 뷰의 세로 구성을 선언한다. |
| `AppBodyView(backend: document)` | 기존 백엔드 인스턴스의 차용 참조를 전달한다. |
| `Footer(status: document.status)` | 푸터 입력을 해당 인스턴스가 노출하는 속성에 바인딩한다. |
| `PushButton(document.save)` | 연결된 메서드를 첫 번째 입력에 전달한다. 클릭 시 호출은 예제 버튼의 콜백 계약으로 정의한다. |
| `enabled: document.canSave` | 접근 가능한 공개 멤버를 읽고 속성 관찰 계약을 통해 변경을 관찰한다. |
| `type: .borderless` | 버튼 외형을 설정한다. |

이름이 붙은 애플리케이션 뷰는 예제이다.
이 문서는 해당 뷰의 선언과 공개 입력 스키마를 구현하지 않는다.

<a id="event-connection-and-property-binding"></a>

## 이벤트 연결과 속성 바인딩

`document.save`는 인스턴스에 연결된 메서드 참조이다.
예제 `PushButton` 인터페이스는 첫 번째 입력으로 콜백을 받고,
버튼 구현이 허용된 클릭에 대해 이를 호출한다.
버튼 생성은 콜백을 실행하지 않고 전달·연결한다.
클릭 의미는 컴포넌트에 속하며 언어는 일반 멤버 접근과 타입에 맞는 인자 전달을 처리한다.
호출이 완료된 뒤에도 소유자와 사용이 활성 상태이면 기존 백엔드를 계속 사용할 수 있다.

`document.status`와 `document.canSave`는 뷰 입력으로 사용할 때 기본적으로 상태 변경을 따르는 공개 멤버 읽기이다.
AOT·네이티브 연동은 의존성 추적과 변경 정보를 제공해야 한다.
공개 접근에는 export 표식이 필요하지 않으며 기본 반응성에는 관찰 어노테이션이 필요하지 않다.

가져온 백엔드가 ViewModel 역할을 하면 상태는 기본적으로 뷰 바인딩에 참여한다.
뷰는 `vm.value` 같은 멤버를 직접 사용하며 바인딩 어노테이션이나 래퍼 별칭은 필요하지 않다.
편집 가능한 바인딩은 공개 계약이 쓰기를 허용하는 경우 같은 VM에 변경을 전달한다.
읽기 전용 멤버는 읽기 전용 계약을 유지한다.
[기본 ViewModel 바인딩](ViewModelQueries.md#default-viewmodel-binding)을 참조한다.

[증가·초기화 예제](examples/ReactiveCounter.lvui)는 `Column { ... }`과 `Label(text: vm.value)`를 사용한다.
`vm.value`가 바뀌면 기본 바인딩을 통해 해당 레이블을 갱신한다.
`@Static`은 해당 수명 동안 고정되는 값을 명시적으로 선택한다.
일반 LVUI 변수도 같은 기본 반응형 모델을 따르며 VM 멤버에만 한정되지 않는다.

VM은 [Core 객체 쿼리](ViewModelQueries.md)를 통해 값을 송수신한다.
어댑터는 공개 멤버 읽기·동작을 값·호출 쿼리에 연결할 수 있다.
변경 쿼리는 필요한 최신 값 읽기를 유발한다.
뷰 파일을 위한 별도 전송 구문은 채택하지 않았다.

멤버 읽기와 메서드 참조는 명령을 암묵적으로 실행하지 않는다.
명시적 호출 표현식은 인자 값을 얻는 용도로 사용하더라도 평가 시 호출을 시작한다.
콜백이 필요한 곳에 반환값 없는 호출 결과를 전달하면 타입 오류이다.
호환되는 callable을 반환하는 경우는 유효할 수 있다.
정확한 표현식 규칙과 반응형 문맥의 효과 제한은 미정이다.

<a id="view-updates-and-identity"></a>

## 뷰 갱신과 정체성

같은 활성 소유 스코프에서 다시 그리기·레이아웃 재계산·속성 재평가는 가져온 인스턴스를 유지한다.
뷰의 다른 인스턴스를 활성화하면 다른 백엔드 인스턴스를 생성한다.
스코프를 제거한 뒤 다시 활성화하면 새 활성화를 위한 새 백엔드를 생성한다.

여러 자식 뷰에 `document`를 전달하면 같은 인스턴스의 사용을 공유한다.
각 자식에서 별도 import를 선언하면 독립적으로 소유하는 인스턴스를 생성한다.

소유권은 [TypeImports.md](TypeImports.md),
종료는 [InvocationLifecycle.md](InvocationLifecycle.md)를 참조한다.

<a id="open-questions"></a>

## 미정 사항

- 채택한 선언·세미콜론 생략 규칙 외의 전체 문법, 줄 경계, 여러 줄 이어쓰기, 주석, 식별자 규칙.
- 제공된 `Column { ... }` 예제 외의 전체 빈 괄호 생략 규칙과 사용자 정의 컴포넌트 선언 방식.
- 열거형·토큰·이름 지정 입력 구문.
- 중복 속성 처리와 괄호 입력·중괄호 본문 속성 사이의 관계.
- 속성 타입·기본값·바인딩 투영 구문·허용 표현식.
- 레이아웃 크기·넘침·테마 상속·컴포넌트 지역 상태.
- 동적 목록 정체성·조건부 뷰 활성화·상태 보존.
- 메서드 인자·오버로드 선택·명시적 이벤트 처리 블록.
- 현재 [HTML 형태의 언어](Language.md)와의 호환 또는 마이그레이션.

채택한 예제에는 `view` 키워드와 명시적 `onClick` 속성이 필수적이지 않다.
해당 구문은 아직 정하지 않았다.
