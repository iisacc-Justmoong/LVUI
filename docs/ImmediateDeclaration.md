# 즉시 선언형 View 구문

LVUI 소스는 태그나 HTML 문서가 아니라 `HStack(...) { ... }`처럼 View를 직접
선언한다. 괄호의 이름 있는 입력과 본문의 속성·자식 View를 조합하며, 본문 항목은
쉼표 또는 줄바꿈으로 구분한다. 한 View의 같은 입력을 여러 번 지정하지 않는다.

기본 표기는 `Name(생성자 입력) { 본문 인자1, 본문 인자2, ... }`이다.
괄호에는 위치 또는 이름을 지정한 생성 입력을, 중괄호에는 이름을 지정한 입력과
자식 View를 둔다. 예를 들어 `VStack(alignment: .stretch) { spacing: Metric.space8,
Label() { text: "Hello" }, Label() { text: "World" } }`로 작성할 수 있다.
지역 선언과 이벤트 본문은 각자의 스코프와 실행 시점을 유지한다. 임의의 이름 없는
스칼라를 중괄호에 나열하는 형태는 지원하지 않는다. 빈 생성 입력의 `()`는
`VStack { ... }`처럼, 빈 본문의 `{}`는 `Label(text: "Hello")`처럼 생략할 수 있다.
둘 다 비어 있으면 `Label`처럼 View 이름만 쓸 수 있다. View별 필수 입력은 계속 검사한다.

시각 요소는 모두 View이다. 자식을 받는 View의 중괄호에는 다른 View를 직접
인자로 전달할 수 있다. 예를 들어 다음 `HStack`은 `VStack` View 하나를 자식으로
받는다. 자식의 본문이 비어 있으면 `VStack()`처럼 중괄호를 생략할 수 있다.

```lvui
HStack() { VStack() }
```

여러 자식 View는 작성한 순서대로 부모에 연결하고 각 View의 스코프와 수명을
유지한다. 자식의 허용 여부·개수·구체적인 역할은 각 컴포넌트의 콘텐츠 계약을
따른다. 예를 들어 `TabBar`는 `Tab`을 받으며 `Label`은 자식을 받지 않는다.
중괄호가 하나 더 있는 `HStack() { VStack() }}`는 오류이다.

배치에는 `HStack`, `VStack`, `ZStack`을 사용한다. 자식은 각각 가로 방향,
세로 방향, 겹친 순서로 배치된다. 세 스택은 서로 중첩할 수 있으며 `gap`과
정렬 등 배치 입력은 해당 iiGUI 스택으로 전달된다. HTML `div`는 내장 View가
아니다. 앱에서 별도로 선언한 사용자 View의 이름은 이 규칙과 무관하다.

```lvui
Window {
    ZStack(alignment: .topLeading) {
        VStack { Label(text: "Background") }
        HStack { Label(text: "Foreground") }
    }
}
```

```text
import vm ToolbarModel
const RandomString: () -> String = vm.randomString
const random = vm.icon

Window {
    HStack(id: toolbar) {
        gap: Metric.space8,
        IconButton(label: RandomString(), icon: random) {
            onClick: vm.activate()
        }
    }
}
```

위 이름과 메서드는 예제의 C++ 객체가 제공해야 한다. `id: toolbar`는 선언 시
고정되는 식별자이다. `gap`은 스택의 간격 입력, `label`은 아이콘 버튼의 텍스트
입력이다. 실제 토큰과 C++ 속성·호출의 타입은 사용 가능한 카탈로그와 컴파일러가
검사한다. `RandomString()`은 값 표현식을 읽을 때 평가한다. 이벤트 입력의
`vm.activate()`는 View를 만들 때 실행하지 않으며 해당 이벤트가 발생한 시점에
호출한다. 종료된 View에 전달된 오래된 이벤트는 객체를 다시 살리지 않는다.

호출 결과가 콜백인 C++ 팩토리를 이벤트에 연결할 때는 구분하여
`onClick: callback(vm.makeHandler())`라고 쓴다. `callback(...)`은 팩토리의 반환
함수를 연결하는 기존 반응형 수명 계약을 사용하며, `onClick: vm.method()`는
클릭 시 메서드 자체를 호출한다. 서명이 있는 `const`에 팩토리 결과를 보관한 뒤
그 이름을 이벤트에 전달할 수도 있다.

LVUI 컴파일 및 문법 검사는 View 선언을 받는다. 앱은 네이티브 AOT 결과를 iiGUI와
iiSDKCore에 연결하며, HTML 파서나 브라우저를 실행 경로에 두지 않는다.

## 태그 없는 이벤트 스크립트

같은 LVUI 파일에서 선언한 화면 상태와 가져온 C++ 객체를 이벤트 본문에서 사용할
수 있다. 본문은 별도의 `<script>` 태그 없이 문장 순서대로 작성하며, 해당 이벤트가
접수된 때에만 실행한다.

```text
import vm PanelModel
@State var expanded: Bool = false

Window {
    PushButton(text: "Open", onClick: {
        expanded = true
        vm.didOpen(expanded)
    })
}
```

이벤트 실행은 뷰 활성화와 같은 소유 스코프에서 이루어진다. 스코프가 종료되면
아직 실행하지 않은 나머지 문장을 실행하지 않으며 C++ 객체를 되살리지 않는다.
앱 상태와 업무 메서드의 구현은 C++ 뷰모델이 소유한다. 코드는 빌드 시 C++23으로
변환하고, 실행 중 소스를 해석하거나 별도의 스크립트 런타임을 시작하지 않는다.
이름 붙인 메서드나 스크립트 본문은 선언하지 않는다. 재사용할 동작은 C++에서
구현하고 import한 메서드 참조를 연결한다. `quitApplication()`과 기존
장면·문자열·쿼리 명령도 허용된 스코프에서 사용할 수 있다.

이벤트 본문에서는 LVUI 자체 문법의 `if`·`else if`·`else`와 `for`를 사용할 수
있다. 조건은 Bool이며 선택한 분기만 실행한다. 반복은 C++ 컬렉션 또는 LVUI 목록을
대상으로 하고, 항목은 본문에서만 유효한 읽기 전용 값이다. C++ 원본은 크기를
알 수 있는 범위(`sized_range`)여야 한다.

```lvui
PushButton(text: "Open selected", onClick: {
    if (vm.ready) {
        for (item in vm.items) {
            if (item.enabled) { vm.open(item.id) }
        }
    } else {
        vm.showNotReady()
    }
})
```

반복의 원본 식은 이벤트가 발생한 시점에 한 번 평가한다. 본문이 원본 컬렉션을
변경해도 반복 순서와 항목이 무효화되지 않도록 복사 가능한 항목의 스냅샷을 사용한다.
스냅샷을 수집하는 동안 원본 구조를 변경해서는 안 되며, 다른 스레드의 접근은
C++ 소유자가 동기화해야 한다. 참조·포인터가 가리키는 객체까지 깊게 복사하지는
않는다. `break`·`continue`는
가장 안쪽 반복에서만 사용할 수 있고, 인자 없는 `return`은 현재 이벤트 블록을
끝낸다. 반복 이름은 기존 스코프 이름이나 이벤트 제어어와 겹칠 수
없다. 타입을 알 수 없는 빈 `[]` 대신 타입이 있는 빈 C++ 컬렉션을 전달한다.
호출 인자와 이항 연산의 피연산자는 왼쪽부터 한 번씩 평가하며 `&&`·`||`는
단락 평가한다. 조건·쿼리 요청·스냅샷 계산과 반복 중에도 뷰가 퇴역하면 남은
피연산자와 다음 작업을 중단한다. 가져온 C++ 메서드의 참조 반환값은 그 참조의
수명을 보장하는 소유 객체가 살아 있어야 하며 임시 인자에 대한 참조를 반환해서는
안 된다.

이 구문은 이벤트 처리 시 C++로 실행된다. 반복 검사를 주기적으로 예약하거나 일반
JavaScript를 해석하지 않는다. 앱 객체와 업무 메서드의 구현은 C++ 뷰모델이
소유한다. 조건과 반복 원본에는 값을 읽는 C++ 쿼리·getter를 사용하고, 변경 작업은
명시적인 이벤트 호출로 둔다. 이벤트 본문 내부의 지역 `const`·`var` 선언과 임의의
`while` 반복은 현재 지원 범위에 포함되지 않는다. 값을 반환해야 하는 `onInput`과
`onCloseRequested`에는 결과를 반환하는 C++ 호출 대상을 연결한다.
