# 컴포넌트 언어의 실제 구현

2026-10-22에 추가한 C++23 컴포넌트 언어 프런트엔드이다.
`LVUI/Language.h`와 `lvui-check`는 실제로 빌드·실행되며, 예약어와 컴포넌트 구문을 검사한다.
기존 HTML 형태의 `lvui::parse`와 `Document` 계약은 유지한다.

## 예약어의 근거

LVRS `src/`의 소스 213개 파일을 읽어 파일 해시와 타입 선언 149개를
`catalog/lvrs-components.json`에 기록했다. QML 파일 91개, 그 내부 컴포넌트 선언 25개와
C++ QML 등록 타입 33개를 대조했다. 루트·인라인 상속과 CMake 등록을 함께 확인한다.

시각 타입 선언 108개 중 중복 이름을 합친 **107개**를 `View` 생성 예약어로 등록했다.
여기에는 QML 파일 타입 81개, C++ 시각 타입 2개, 추가 고유 인라인 이름 24개가 포함된다.
내부 타입도 포함하며 `MaterialSurface`처럼 여러 원본에서 같은 이름으로 선언한 경우
모든 원본을 조사 파일에 남기고 예약어는 한 번만 등록한다.

`InteractionMotion`, `SpringBehavior`, `StateColorBehavior`는 시각 변환·동작 보조 예약어이다.
`View`, `import`, `let`, `var`, `true`, `false`를 포함하면 전체 예약어는 **116개**이다.
`Theme`, `Motion`, 모델·서비스·상태 등 비시각 객체 38개 선언은 시각 예약어에서 제외한다.
전체 이름과 원본 위치는 [예약어 목록](ReservedWords.md)에 있다.

이 목록은 LVRS가 직접 선언한 타입을 조사한 것이다. LVRS 내부에서 사용하는 Qt 기본 타입
자체를 LVUI 의존성으로 가져오지 않는다. 일반 빌드·설치 소비자는 LVRS·QML·Qt가 필요하지 않다.

## 공개 API

```cpp
#include <LVUI/Language.h>

const auto* keyword = lvui::find_keyword("PushButton");
// keyword->kind == lvui::KeywordKind::view
// keyword->source_file / source_line: 조사한 LVRS 선언 위치

auto tokens = lvui::tokenize("VStack { Label(text: \"Hello\") }");
auto document = lvui::parse_view("VStack { Label(text: \"Hello\") }");
if (document) {
    const lvui::View& root = document->root();
    // root.component->spelling == "VStack"
    // root.children[0].component->spelling == "Label"
}
```

`keywords()`는 정렬된 정적 디스크립터 목록을 반환하고 `find_keyword`는 이진 탐색한다.
테이블의 정렬과 이름 유일성은 컴파일 타임에도 검사한다.
`is_reserved_identifier`는 정확한 대소문자로 이름을 검사한다.
`PushButtonSuffix`나 소문자 `pushButton`은 `PushButton` 예약어가 아니다.

`tokenize`는 위치가 있는 소유 토큰 목록을, `parse_view`는 소유 `ViewDocument`를 반환한다.
뷰는 정적 컴포넌트 디스크립터, 일반 입력, `{}`의 자식 `View`, 지역 선언·import를 보관한다.
문자열·식별자·값은 원본 소스가 제거되어도 유효하다. 디스크립터의 수명은 프로세스 전체이다.
오류는 `std::expected`의 `Diagnostic`으로 전달하며 바이트 위치와 Unicode 스칼라 기준 행·열을 제공한다.

## 구현한 구문

```text
import vm CounterBackend
let title: String = "Counter"
var value: Int = 6

ApplicationWindow(title: title) {
    VStack(spacing: 16) {
        Label(text: vm.value)
        PushButton(vm.increment) {
            text: "+"
            enabled: true
        }
        Tab(iconName: "home", text: "Home")
    }
}
```

- 한 문서는 루트 `View` 하나를 가지며, 선언과 import는 루트 앞이나 뷰 본문에 둘 수 있다.
- 컴포넌트 이름은 등록된 시각 예약어여야 한다. `{}`로 다른 `View`를 자식 인자로 전달한다.
- `VStack() { ... }`와 `VStack { ... }`를 지원한다. 인자가 없는 생성에도 `()` 또는 `{}`가 필요하다.
- `()`는 위치 인자와 이름 지정 인자를 받으며, 위치 인자는 이름 지정 인자보다 앞에 둔다.
- 본문 속성 `text: "Save"`도 입력으로 보관한다. 괄호와 본문 사이에서도 같은 입력 이름을 중복하면 오류이다.
- 값은 UTF-8 문자열, 유한 숫자·음수, 불리언, `.blue` 같은 열거 표기, 점으로 연결한 참조,
  위치 인자를 사용하는 호출과 목록이다. 문자열 escape는 `\\`, `\"`, `\'`, `\n`, `\r`, `\t`이다.
- `vm.method`와 `vm.method()`는 각각 참조와 호출 AST로 구분한다. 파싱으로 메서드를 실행하지 않는다.
- `let`·`var`는 스칼라·참조·목록·호출 초기화 구문을 보관한다. 타입 표기도 보관하며 전체 타입 검사는 별도 과제이다.
- import 바인딩·변수·컴포넌트 입력 이름에 예약어를 쓰면 오류이다. 같은 스코프의 바인딩 중복도 거부한다.
- 문장 사이에는 줄바꿈을 사용한다. `//`·`/* ... */` 주석과 CRLF를 지원하며 세미콜론은 허용하지 않는다.
- 입력은 최대 1 MiB, 소스 토큰은 65,536개, `View`는 4,096개이며 뷰·표현식 깊이는 64단계이다.

`View()` 자체는 구체적인 시각 컴포넌트 생성자가 아니다.
`View` 타입 변수의 뷰 초기화, 사용자 정의 컴포넌트 선언, 어노테이션,
임의 연산자·람다·이벤트 처리 블록, 컴포넌트별 입력 타입·필수 인자 검사는 아직 구현하지 않았다.
이 범위 밖의 구문은 무시하지 않고 진단한다.

## 명령행 검사와 검증

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/install"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/lvui-check docs/examples/ReservedComponents.lvui
./build/lvui-check --keywords
cmake --install build
```

`lvui-check`는 성공하면 0, 구문 오류이면 행·열과 진단을 출력하고 1,
파일·사용법 오류이면 2를 반환한다. [실제 검사 예제](examples/ReservedComponents.lvui)와
[카운터 예제](examples/Counter.lvui)를 CTest로 검사한다.

소스 조사 테스트는 현재 LVRS 소스와 스냅샷을 대조한다. LVRS 소스가 없는 환경에서도
스냅샷과 생성된 예약어의 일치, 전체 예약어와 파서, 이름 충돌, 입력 한계는 검사할 수 있다.
Python은 조사·검증 도구에만 사용하며 C++ 라이브러리와 명령행 도구의 실행 의존성이 아니다.

## 구현 경계

이 프런트엔드 API는 컴파일 단계의 어휘·구문 분석과 `View` AST를 제공한다.
파싱은 실제 C++ 객체를 생성하거나 메서드를 호출하거나 화면을 표시하지 않는다.
[별도의 실제 AOT 경로](NativeAOT.md)는 `compile_native`와 `lvui-compile`을 통해
일부 컴포넌트를 C++23으로 생성하고 iiGUI의 자체 위젯에 마운트한다.
공통 위젯이 배치·입력·그리기를 소유하며 Cocoa는 창·입력·표면을 제공한다.
[현재 목록](LVRSNative.md)과 나머지 예약어를 구분한다. Windows·Linux 위젯 입력 어댑터,
추가 GPU 코드 생성과 일반 반응형 어댑터는 계속 구현해야 한다.
예약어로 등록했다는 사실은 모든 컴포넌트의 네이티브 렌더링과 동작을 구현했다는 뜻이 아니다.
