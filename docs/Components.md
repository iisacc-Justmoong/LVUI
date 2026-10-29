# 타입 매개변수가 있는 재사용 뷰

`let Name(...): View = ...`는 빌드 시 확장하는 재사용 구성을 정의한다. 새로운 런타임
함수나 스크립트 객체를 생성하지 않으며, 최종 입력·파생식·콜백은 C++23으로 컴파일한다.
매개변수가 없는 기존 [View 값](ViewValues.md)의 루트 입력 덮어쓰기와 구분한다.

```lvui
import class Domain.Model
import document Domain.Model

let Status(backend: Model, title: String = "Document"): View = VStack {
    Label(text: title)
    Label(text: backend.title)
    PushButton(backend.increment, text: "Increment")
}

Window {
    HStack {
        Status(backend: document)
        Status(backend: document, title: "Second view")
    }
}
```

매개변수는 이름과 타입을 필수로 선언한다. 호출 인자는 이름을 지정하며, 필수 인자 누락·
중복·알 수 없는 이름·위치 인자·알려진 타입 불일치는 소스 위치와 함께 거부한다.
서명은 최대 64개 매개변수를 받으며, 기존 구성 확장 깊이 64단계와 위젯 수 4,096개
제한을 유지한다. `let Name(): View = ...`도 명시적인 빈 서명이다.

## 스칼라 입력과 기본값

현재 스칼라 타입은 `String`, `Bool`, `Int`, `Float`, `Double`, `Metric`, `Color`이다.
일반 입력은 반응형이며, 의존 원본이 바뀌면 기존 위젯의 값을 갱신한다. `Int`는 소수나
정확히 표현할 수 없는 정수를 허용하지 않는다. 알려진 스칼라 타입은 LVUI에서 검사하고,
직접 접근하는 C++ 멤버의 최종 타입·접근성·호출 가능성은 생성 C++의 타입 검사도 적용한다.
Color는 [공유 색상 값·네이티브 바인딩](NativeColors.md)을 따르며 일반 값처럼 원본 변경을
추적한다. 색상 기본값은 정의 환경의 토큰과 앞선 입력을 사용한다.

```lvui
let Heading(title: String, subtitle: String = title): View = VStack {
    Label(text: title)
    Label(text: subtitle)
}
```

전달한 식은 호출 위치의 어휘 스코프를 포획한다. 생략한 인자의 기본값은 **정의 위치**와
앞서 연결한 매개변수를 사용한다. 전달한 인자는 선언 순서와 관계없이 호출자의 환경에서
먼저 포획하므로, 앞선 매개변수가 같은 이름을 사용해도 뒤에 전달한 식의 수신자가 바뀌지
않는다. 기본값의 유효성은 해당 구성을 확장할 때 검사한다.

매개변수 자체는 읽기 전용 입력이다. 구성 내부의 `@Static let`은 활성화 당시의 값을
고정하며, 일반 파생 선언은 변경을 따른다. 상수로 결정되는 창 입력은 빌드 시 값으로
사용하고, 동적 제목·크기·테마는 [네이티브 Window 입력](NativeWindow.md)을 따른다.

동적 스칼라 인자는 활성화에 속한 **하나의 읽기 전용 값**이다. 예를 들어
`Heading(title: vm.compute(vm.count))`를 호출하고 본문에서 `title`을 여러 번 표시해도
각 위젯이 `compute()`를 따로 실행하지 않는다. 최초 활성화에 한 번 계산하며 추적한
원본 변경에 따라 다시 계산한다. 같은 입력을 사용하는 앞선 매개변수의 기본값·자식
구성·`[View]`의 여러 항목도 현재 결과를 읽는다. 개별 호출 위치의 활성화들은 독립적이다.
상수 입력은 계속 빌드 시 결정하여 창의 정적 생성 옵션에 사용할 수 있다.

앞선 함수 매개변수를 호출한 결과도 뒤쪽 스칼라 입력의 기본값이 될 수 있다.
`render: (Int) -> String, title: String = render(vm.count)`에서 함수 버전이나 `count`가
바뀌면 `title` 한 값을 갱신한다. 이를 표시하는 여러 위젯 때문에 팩터리나 함수가
소비자 수만큼 실행되지 않는다. 자세한 저장·수명·오류 계약은
[공유 스칼라 입력](ScalarInputs.md)에 정리한다.

## C++ 백엔드 차용

청사진에 별칭을 줄 때에는 `import class Row lvrs.HierarchyItem`처럼 선언한다.
별칭은 현재 어휘 스코프의 타입 이름이며 C++ 객체를 생성하지 않는다. 기존
`import class Domain.Model`의 자동 짧은 이름도 유지한다. C++ 타입 경로의 이름은
LVUI의 뷰·타입 이름과 겹칠 수 있으며, 이때 예약된 짧은 이름을 선언하지 않도록
명시적 별칭을 사용한다. 일반 `import instance Cpp.Type`의 인스턴스 생성과 구분한다.

백엔드 타입은 정의 환경의 `import class` 청사진을 사용한다. 같은 C++ 타입의 기존
import 인스턴스나 앞서 차용한 매개변수를 전달하면 원래 인스턴스를 차용한다. 타입
청사진 자체를 값으로 사용할 수는 없다. 현재는 정규화한 import 타입의 일치를 검사하며
임의 C++ 상속 변환을 추정하지 않는다.

메서드가 반환한 복사 가능한 구조체나 객체 핸들은 [C++ 구성 입력](ObjectInputs.md)의
별도 값 경로로 전달한다. 기존 백엔드를 새로 만드는 기능이 아니며, 입력을 사용하지
않는 구성에서도 선언한 C++ payload 타입과 복사·소유권 계약을 검사한다.

동일한 `document`를 여러 뷰에 전달해도 생성·복사·소유권 이전을 수행하지 않는다.
중첩 컴포넌트로 다시 전달해도 같은 객체와 변경 발행 경로를 사용한다. 자식의 지역 import가
같은 이름을 가리더라도 이미 포획한 인자는 원래 객체를 유지한다.

자식 콜백은 해당 활성화의 접수 경계를 따르며, 부모가 닫히면 차용 접근도 닫힌다. 조건부
자식 제거는 자식 소유 import와 구독을 퇴역시키며, 살아 있는 부모 객체는 유지한다.
재활성화는 새 자식 스코프와 고정 스냅샷을 생성하고 기존 부모를 다시 빌린다. 비동기 작업
전체의 대기·취소·실행 중 사용 종료까지 이 구성 기능으로 완료되었다고 간주하지 않는다.

## 이름이 있는 View 슬롯

`View` 매개변수는 이름이 있는 콘텐츠 슬롯이다. 기존 View 값·인라인 구성·기본 View를
받고 구성 내부에서 `content()`처럼 사용한다.

```lvui
let Empty: View = Label(text: "No content")
let Panel(title: String, content: View = Empty): View = VStack {
    Label(text: title)
    content()
}
Window {
    Panel(title: "Settings", content: HStack {
        Label(text: "Native content")
    })
}
```

슬롯은 호출 위치의 View 정의와 데이터 환경을 유지한다. 같은 슬롯을 여러 번 사용하면
사용 위치별 위젯·ID 영역을 갖는다. `TabBar`처럼 자식 타입을 제한하는 컴포넌트는 슬롯이
확장한 실제 뷰 타입을 검사한다. 슬롯으로 타입 제한을 우회하지 않는다.

명시적인 서명이 있는 구성은 선언한 인자만 받는다. 호출 뒤 `{ ... }`로 기본 자식을
암묵적으로 대체하지 않으며 `content: ...`처럼 슬롯 이름으로 전달한다.
[타입 지정 이벤트 콜백](CallableInputs.md)은 `action: () -> Void`와 같은 입력으로 전달한다.
읽기 전용 함수 선언과 결과가 있는 입력은 [반환 함수 값](FunctionValues.md)을 따른다.
[순서가 있는 구성 슬롯](ViewSequences.md)은 `content: [View]`로 빈 목록·복수 뷰·
중첩 목록을 받는다. 펼친 실제 자식을 기준으로 Window·Modal의 루트 수량과 TabBar·
Menu의 자식 타입을 검사한다. 가변 View 교체와 일반 슬롯 프로토콜 제약은 추가 구현
범위이다.
키를 유지하는 [목록 반복](Repetition.md)은 별도로 제공한다. 고정 크기 목록·그리드는
네이티브 뷰포트에 따른 자동 가상화, [가변 행 측정](MeasuredCollections.md)과
[화면 밖 탐색·선택](CollectionNavigation.md)도 연결한다. 전체 슬롯 타입·프로토콜 계약은
별도의 완료 검사 대상으로 남아 있다.

## 검증 경계

`LVUI.Components.*`는 구문·타입·명시적 차용·슬롯·기본값·루트·진단·입력 한계를 검사한다.
`LVUI.NativeComponents`는 생성된 다단계 C++ 스코프의 입력·파생값·고정값을 UI 없이
실행하며, 메모리·정의되지 않은 동작 검사 구성에도 포함한다.
`iiGUI.native_components`는 실제 AOT 결과를 C++로 컴파일·링크하여 창 없는 위젯 상태에서
동일 백엔드 사용, 입력 갱신, 정적 스냅샷, 콘텐츠 포획, 종료 및 조건부 재활성화를 실행한다.
자신의 실행 중인 메서드가 조건부 뷰를 제거하는 경우에도 반환 전 수신자를 보존하고,
반환 후 자식 객체를 해제하며 이후 호출을 거부하는지 함께 확인한다.
이는 실제 창·GPU·키보드·접근성 및 LVRS 전체 동작의 최종 검증과 별개이다.

`LVUI.NativeInstalled`는 `build/` 내부 설치·경로 이동 후 독립 CMake 소비자에서 네이티브
바인딩뿐 아니라 공개 파서·컴파일러의 타입 매개변수·차용·슬롯 구문도 컴파일·링크·실행한다.

### 2026-10-26 검증 기록

현재 macOS arm64 작업 트리에서 다음 결과를 확인했다. 서로 겹치는 검사를 합산하여
독립 기능 수나 전체 플랫폼 지원 수로 표시하지 않는다.

| 검사 | 결과 | 범위 |
|---|---|---|
| LVUI Release CTest | 103/103 통과 | 기존 언어·AOT·관측·설치 소비자와 신규 구성 검사 |
| AddressSanitizer + UndefinedBehaviorSanitizer | 32/32 통과 | 구성·View 값·조건부·네이티브 바인딩과 생성 C++ 스코프 |
| iiGUI 창 없는 통합 검사 | 4/4 통과 | 퇴역·조건부 교체·View 입력·타입 매개변수 구성 |
| 설치·재배치 독립 소비자 | 2/2 통과 | 네이티브 바인딩 및 공개 컴포넌트 파서·컴파일러 |
| 실행 파일 의존성 | Qt 동적 라이브러리 없음 | `lvui-compile`, `LVUINativeComponentTests`의 `otool -L` |
| Dreamscapes 읽기 전용 관측 | QML 81개 해시 변경 0 | 별도 Dreamscapes 작업 이후 설정한 이 작업 구간의 기준선 |

주요 테스트 소스는 [구문·차용·슬롯 검사](../tests/components.cpp),
[생성 C++ 스코프 검사](../tests/native_components.cpp),
[실제 위젯 구성 검사](../../iiGUI/tests/native_components.cpp)이다. 설치 소비자는
[독립 CMake 프로젝트](../tests/native_consumer/CMakeLists.txt)를 사용한다.

모든 생성·빌드·설치 스테이징은 `build/` 하위에 두었다. 실제 창·GPU·입력·접근성과
LVRS 앱·테스트 실행은 전체 작업의 최종 일괄 검증 범위로 유지한다. 이 기록은
[전체 언어·모듈 완성](Completion.md)을 선언하는 것이 아니다.
