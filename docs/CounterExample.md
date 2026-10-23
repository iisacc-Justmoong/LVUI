<a id="counter-view-example"></a>

# 카운터 뷰 예제

상태: 채택한 스코프 기반 백엔드 설계의 가상 뷰 예제이다.
소스는 [examples/Counter.lvui](examples/Counter.lvui)이다.
`lvui-check`의 구문 검사 대상으로 검증한다. C++ 백엔드·이벤트·화면을 실행하는 예제는 아니다.

<a id="view-structure"></a>

## 뷰 구조

애플리케이션 창에는 카운터 제목, 현재 숫자에 바인딩된 레이블,
가로로 배치한 감소·증가 버튼이 있다. 두 버튼은 숫자 레이블과 같은 가져온 백엔드 인스턴스를 사용한다.

예제는 [선언적 표기](DeclarativeSyntax.md)와 [스코프 기반 import 계약](TypeImports.md)을 따른다.
클릭마다 새 백엔드 인스턴스를 생성할 필요는 없다.

import는 `import counter CounterBackend`이다.
뷰에서 사용할 이름 `counter` 뒤에 C++ 백엔드 타입 `CounterBackend`가 온다.

<a id="assumed-c-backend"></a>

## 가정한 C++ 백엔드

예제는 C++ 타입 `CounterBackend`가 다음 공개 인터페이스를 제공한다고 가정한다.
import나 이 멤버들에 대한 접근에 `@export` 어노테이션은 필요하지 않다.

| 멤버 | 계약 |
|---|---|
| `value` | 0으로 초기화하며 읽기·관찰 가능한 정수 속성이다. |
| `increment()` | `value`를 1 늘리고 변경을 알린다. 인자와 반환값은 없다. |
| `decrement()` | `value`를 1 줄이고 변경을 알린다. 인자와 반환값은 없다. |

변경 전파는 [기본 반응형 값 모델](ReactiveState.md)의 요구사항이다.
숫자 레이블이 `value`를 따라가기 위한 관찰 어노테이션은 필요하지 않다.
네이티브 어댑터는 필요한 읽기·변경 연동을 구현해야 한다.
비공개 저장소를 유지하면서 공개 getter로 읽기 전용 상태를 제공할 수 있다.
getter와 속성 표기의 정확한 연결 방식은 아직 설계 세부사항이다.

`counter.increment`와 `counter.decrement`는 인스턴스에 연결된 메서드 참조이다.
예제는 `PushButton`이 첫 번째 입력으로 인자·반환값 없는 콜백을 받고,
허용된 클릭에 대해 호출한다고 가정한다. 뷰 생성은 콜백을 전달하고 연결하며 실행하지 않는다.
클릭 의미는 컴포넌트가 소유하고, LVUI는 일반 참조와 인자 전달을 처리한다.

`counter.increment()`는 평가 시 메서드를 명시적으로 호출한다.
반환값 없는 결과는 해당 콜백 입력을 충족하지 못하므로,
콜백 인자에 이 호출을 배치하면 AOT 타입 오류가 된다.
[MethodReferences.md](MethodReferences.md)를 참조한다.
예제는 음수를 허용하며 재설정이나 영속 저장 동작은 추가하지 않는다.

이 산출물은 C++ 백엔드를 구현하거나 등록하지 않는다.

<a id="query-based-value-exchange"></a>

## 쿼리 기반 값 교환

이 가상 예제에서 `CounterBackend`는 ViewModel 역할을 한다.
숫자 읽기·명령·변경 알림에 [Core 쿼리 연동](ViewModelQueries.md)을 사용할 수 있다.

1. 어댑터의 쿼리 계약으로 선택한 `value`를 읽어 `0`을 얻는다.
2. 버튼 클릭 시 증가·감소 호출 쿼리를 보낸다.
3. VM의 실행 영역에서 변경을 적용하고 변경 쿼리를 발행한다.
4. 필요한 최신 값을 읽어 UI 영역에서 레이블에 전달한다.

Core의 `PropertyRead`, `PropertyInvoke`, `PropertyChange`를 구성 요소로 사용할 수 있다.
정적인 공개 멤버 매핑은 여기서 구현하지 않는다.
변경 기록은 새 값을 자동으로 포함하는 대신 경로·리비전을 담는다.
이 과정 내내 인스턴스는 동일하다. `.lvui` 예제에 저수준 쿼리 구문을 추가하지 않는다.

카운터 VM은 기본 바인딩을 제공한다. 바인딩 어노테이션이나 래퍼 별칭 없이
레이블에 `counter.value`를 직접 사용한다. 네이티브 어댑터는 필요한 읽기·변경 연동을 제공해야 한다.
기본 동작은 두 번째 카운터를 생성하거나 읽기 전용 값을 쓰기 가능하게 바꾸지 않는다.

<a id="lifetime-and-expected-interaction"></a>

## 수명과 예상 상호작용

1. 소유 뷰를 활성화하고 `CounterBackend` 인스턴스 하나를 생성한다.
2. 숫자 레이블을 `counter.value`에 바인딩하고 두 버튼을 연결한다.
3. 모든 클릭에서 같은 인스턴스를 재사용하고 속성 변경을 레이블에 반영한다.
   `initial -> plus -> plus -> minus` 상호작용은 `0 -> 1 -> 2 -> 1`을 표시한다.
4. 소유 뷰를 제거하고 사용 연결을 해제한 뒤, 활성 사용이 종료되면
   [수명 계약](InvocationLifecycle.md)에 따라 백엔드를 파괴한다.

별도 활성화는 0으로 초기화한 독립 카운터로 시작한다.

제공된 `Column` 증가·초기화 예제는 [ReactiveState.md](ReactiveState.md)와
[examples/ReactiveCounter.lvui](examples/ReactiveCounter.lvui)에 별도로 기록한다.

<a id="example-only-notation-assumptions"></a>

## 예제에 한정한 표기 가정

`Label`과 `HStack`은 [실제 시각 예약어](ReservedWords.md)이다.
창의 `title`과 스택의 `spacing`은 파서가 입력 구문으로 보관하며,
컴포넌트별 입력 스키마와 실행 동작은 별도 구현 과제이다.
`Label(text: ...)`에 정수를 전달하면 해당 값을 10진수 텍스트로 표시한다고 가정한다.
일반적인 숫자 서식이나 변환 규칙은 아직 정하지 않았다.

이 가정들은 가상 파일의 의도를 설명한다.
[구현된 문법](Language.md)을 확장하거나 최종 컴포넌트 API를 선택하는 것은 아니다.
