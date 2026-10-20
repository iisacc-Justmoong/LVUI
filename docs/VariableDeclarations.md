<a id="constants-and-variables"></a>

# 상수와 변수

상태: 2026-10-20에 채택한 언어 설계이다. 현재 LVUI 0.1 파서는 이 선언을 구현하지 않는다.

<a id="declaration-syntax"></a>

## 선언 구문

`let`은 상수, `var`는 변경 가능한 변수를 선언한다.
채택한 타입·초기값 지정 선언 형식은 다음과 같다.

```text
let name: Type = initializer
var name: Type = initializer
```

이름은 선언 키워드 뒤에 온다. 콜론 뒤에 타입을, 등호 뒤에 초기값을 지정한다.

```text
let constant: Int = 6
var variable: Int = 6
```

`constant`는 초기화 이후 재대입할 수 없다. 일반 `variable`은 선언한 타입에 따라 재대입할 수 있다.
`Int`는 채택한 예제의 타입 표기이며, 전체 타입 체계는 추가 명세가 필요하다.

<a id="declaration-annotations"></a>

## 선언 어노테이션

선언 앞에 `@Observable`과 `@Static`을 붙일 수 있다. 예시는 다음과 같다.

```text
@Observable var value: Int = 6
@Static let initialValue: Int = 6
```

값은 [기본 반응형 모델](ReactiveState.md)을 따른다. `@Observable`은 관찰을 명시하지만
관찰 기능을 활성화하기 위한 필수 조건은 아니다. `@Static`은 해당 선언에 설정된 수명 동안
초기값을 고정한다. 채택한 의미·예제와 남은 적용 대상·조합 규칙은
[DeclarationAnnotations.md](DeclarationAnnotations.md)에 기술한다.

<a id="no-semicolons"></a>

## 세미콜론 생략

LVUI는 문장이나 선언을 끝내는 세미콜론을 사용하지 않는다.
위 예제에는 끝의 `;`이 없으며, 선택적으로 붙이는 종결자도 아니다.
동일한 규칙을 import와 주변 컴포넌트 구문에도 적용한다.

줄 경계와 여러 줄 표현식의 전체 규칙은 미정이다.
채택한 컴포넌트 언어의 이 규칙은 자체 구분자가 이미 구현된
[LVUI 0.1 인라인 스타일 형식](Language.md)을 바꾸지 않는다.

<a id="state-and-backend-integration"></a>

## 상태와 백엔드 연동

`let`과 `var`는 선언한 값의 재대입 가능 여부를 지정한다.
뷰에서 사용하는 일반 LVUI `var`를 포함하여 값은 기본적으로 반응형이다.
상태 변경을 의존 바인딩에 전달하기 위한 어노테이션은 필요하지 않다.
`@Static`은 설정된 수명 동안 고정되는 값을 선택한다.

이 기본 동작은 ViewModel 인스턴스를 생성하거나 선언의 소유자 수명을 더 길게 정하지 않는다.
구체적인 상태 저장소·정체성·수명 규칙은 추가 명세가 필요하다.
ViewModel도 같은 일반 값 모델을 따르며 [기본 바인딩](ViewModelQueries.md#default-viewmodel-binding)을 제공한다.
`let`과 C++ constness의 기존 재대입·쓰기 제한은 유지된다.

권장 MVVM 패턴에서는 공유 화면 상태를 ViewModel에 둔다.
지역 변수가 이 원본 상태를 중복해서 보관하지 않도록 한다.
ViewModel은 [Core 객체 쿼리](ViewModelQueries.md)를 통해 값을 송수신하며,
가져온 백엔드의 소유권은 계속 [스코프 기반 import](TypeImports.md)를 따른다.

<a id="open-questions"></a>

## 미정 사항

- 타입 추론, 전체 타입 집합, 초기화 요구사항.
- 컴포넌트 본문 내 선언 스코프와 평가 시점.
- 대입 표현식과 여러 줄 표현식 구문.
- 변경 가능한 C++ 객체·C++ constness와 `let`의 관계를 포함한 참조 값 의미.

채택한 규칙은 상수와 변수의 구분, 예시의 타입·초기값 구문,
접두 어노테이션, 세미콜론 종결자 생략을 확정한다.
