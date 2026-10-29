# Color와 Metric 값 타입

LVUI의 `Color`와 `Metric`은 색상과 스타일 수치를 선택하는 내장 값 타입이다. Figma의 긴 CSS 이름이나 문자열 경로 대신 타입 멤버로 선택한다. `Color` 138개, `Metric` 67개를 [전체 멤버 목록](StyleTokenMembers.md)에 공개한다. 원본 값·ID·별칭·모드·앱별 설정은 [기본 토큰 카탈로그](StyleTokens.md)와 공유한다.

## 선택과 타입 추론

```text
let ink: Color = .primary
let gap: Metric = .space8

ApplicationWindow(backgroundColor: Color.window) {
    VStack(spacing: gap, padding: Metric.space24) {
        Label(text: "Native LVUI", color: ink,
              fontSize: Metric.textBody, fontWeight: Metric.weightMedium)
    }
}
```

타입 멤버의 문법은 `Color.멤버`와 `Metric.멤버`이다. 타입을 명시한 선언이나 색상·수치 입력에서는 `.멤버`로 축약할 수 있다. 위 Label의 색상을 `.primary`, 글자 크기를 `.textBody`로 작성해도 같은 C++ 코드가 생성된다. 기존 컴포넌트 enum인 `style: .body`, `tone: .primary`는 기존 의미를 유지한다.

| 선택 | 원본 |
|---|---|
| `Color.primary` | Theme Colors / Action/primary |
| `Color.window` | Theme Colors / Panel/window |
| `Color.text.body` | Theme Colors / Text/bodyColor |
| `Color.blue`, `Color.blue.hover` | Theme Colors / Accent/Blue/Default, Hover |
| `Color.material.surface.dense.edge` | Material / Surface/Dense/Edge |
| `Color.picker.current` | ColorPicker / Current color |
| `Metric.space8`, `Metric.radius8` | Theme Metrics / Space/8, Radius/8 |
| `Metric.size22`, `Metric.stroke1_5` | Theme Metrics / Size/22, Stroke/1_5 |
| `Metric.textBody`, `Metric.textBodyLineHeight` | Theme Typography / Body의 크기·줄 높이 |
| `Metric.weightMedium` | Theme Typography / Weight/Medium |

접두사를 생략할 타입 문맥이 없는 `let ink = .primary`는 오류이다. `let ink = Color.primary`로 타입을 추론하거나 `let ink: Color = .primary`로 지정한다. 수치 입력은 Number와 Metric을 받으며, Metric을 색상·문자열·Bool로 사용하는 것은 거부한다. 수치 목록 입력에서도 `[Metric.size44, .size56]`처럼 작성할 수 있다.

타입 이름은 예약어이다. 변수나 import 바인딩 이름으로 `Color`·`Metric`을 다시 선언할 수 없다. 멤버는 정적 카탈로그 선택이며 임의 객체의 동적 속성이나 C++ 백엔드 참조로 해석하지 않는다. `Color.primary()`처럼 멤버를 함수로 호출하는 것도 오류이다.

## 직접 값 생성과 상태

```text
let customInk: Color = Color("#12345678")
let customGap: Metric = Metric(12)
let otherInk: Color = "#abcdef"
let otherGap: Metric = 16
var distance: Metric = .space8
```

`Color()`는 Color 값 또는 기존 HEX 문자열 하나, 또는 유한한 0~1 RGBA 채널 네 개를 받는다. `Metric()`은 유한한 Number/Metric을 하나 받는다. 명시적인 타입 선언에서 해당 리터럴을 바로 사용할 수도 있다. `Metric`은 별도 값 종류로 검사되며 수치 스타일을 생성할 때 double로 내려간다. `Int`·`Float`·`Double` 선언을 통해 명시적으로 수치로 받을 수 있고, Int는 기존 정수 검사를 따른다.

Metric `var`는 기존 Core Observable 수치 바인딩과 생성 setter를 사용한다.
[네이티브 Color](NativeColors.md)는 `Color` 타입의 재사용 뷰 입력·일반 let/var·Static
스냅샷·조건식·합성·함수 인자와 결과를 C++ 값과 바인딩으로 연결한다. 색상은 iiColorManager의
공유 값 계약으로 검증하며, 외부 C++ 속성의 실제 타입은 생성 C++ 컴파일러도 검사한다.

Color는 원본 encoded sRGB와 독립 알파를 유지한다. `composite(.material.surface.dense.edge, .window)`는 두 인자를 Color로 추론한다. Metric도 원본 수치·단위를 유지한다. 예를 들어 `Metric.opacity32`는 32이며 0.32로 자동 정규화하지 않는다. 속성별 크기·범위·글꼴 굵기 검사는 계속 적용된다.

## HTML 형태 문법

```html
<ApplicationWindow style="background:Color.window">
  <div style="gap:Metric.space8;padding:.space24">
    <span color={Color.text.body}
          style="font-size:.textBody;font-weight:Metric.weightMedium">Native LVUI</span>
  </div>
</ApplicationWindow>
```

인라인 스타일은 속성의 색상·수치 문맥에서 접두사를 생략할 수 있다. 초기 `<main>` 파서도 기존 지원 속성에서 같은 선택을 해석한다. `.5` 같은 숫자 표기를 스타일 멤버로 재해석하지 않는다. String 토큰 3개는 Color/Metric 멤버에 포함하지 않는다. 글꼴 이름 등에는 기존 `token()` 또는 `var()`를 사용한다.

## C++ 카탈로그와 재생성

```cpp
const auto& catalog = lvui::default_style_tokens();
auto primary = catalog.color("Color.primary");
auto spacing = catalog.number("Metric.space8");
auto selected = catalog.select("Color.material.surface.dense.edge");

lvui::NativeCompileOptions options;
options.style_tokens.overrides["Metric.space8"] = 10.0;
options.style_tokens.overrides["Color.primary"] = lvui::StyleColor{.2, .4, .8, 1};
```

`StyleToken::selector`는 해당 변수의 선택 이름이다. String/Boolean 변수는 비어 있다. `select()`는 Color/Metric 멤버를 검증하고 원본 ID로 조회한다. 기존 ID/key/CSS/컬렉션 경로 조회도 유지한다. `style_value(text, context, options)`는 인라인 스타일의 문맥상 멤버를 해석하고, 기존 `css_value(text, options)` API도 그대로 사용할 수 있다.

선택 이름은 컬렉션·변수 경로에서 결정하므로 다른 토큰을 추가해도 기존 이름을 단독 leaf의 유일성 때문에 변경하지 않는다. 이름 충돌은 생성 오류이며 자동으로 덮어쓰지 않는다. 원본 변수 경로를 이름 변경할 때에는 공개 선택 이름의 변경도 검토해야 한다.

```sh
python3 -B tools/update_style_tokens.py
python3 -B tools/update_style_tokens.py --check
python3 -B tools/update_keywords.py --check
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

생성기는 값 카탈로그와 멤버 문서를 함께 검증한다. 예약어 생성기는 Color/Metric을 type으로 등록한다. AOT는 이 선택을 C++ WidgetColor와 숫자로 확정하므로 실행 중 타입 이름·선택 문자열·LVUI 토큰 해석기를 필요로 하지 않는다.

## 2026-10-25 검증 기록

macOS arm64 Release 빌드와 LVUI 전체 CTest **60/60**이 통과했다. 새 테스트는 205개 선택 이름의 원본 ID 대응, 별칭·값 변경 유지, 타입 예약·명시/문맥상 선택의 동일한 코드 생성, 값 생성자, Metric 반응형 상태, 수치 목록, 잘못된 타입·멤버와 소스 진단 위치를 검사한다. Python 생성기 테스트 10개는 멤버 전체 수량·이름 충돌·문서 드리프트도 포함한다.

`build/style-token-prefix`에 설치한 패키지로 별도 소비자를 다시 빌드했고 **4/4** 테스트가 통과했다. Metal/Vulkan의 실제 하드웨어·sRGB attachment에서 기본 배경 20, 합성 보더 약 48, Pretendard 제목 26·본문 13을 유지했다. HTML 형태 뷰를 새로 마운트한 뒤 Metric 값 8을 생성 setter로 14로 변경하여 같은 네이티브 슬라이더에 반영되는 것도 확인했다. Windows/Linux 실제 실행은 이 기록에 포함하지 않는다.

생성 헤더에는 Color/Metric 선택 문자열이나 LVUI 토큰 해석기가 없으며, 소비자와 연결 Core/FileProvider 라이브러리에 Qt 링크가 없다. 원본 Figma 스냅샷의 SHA-256은 기존 `7d9b1bfa1c345568afe4fea6ba97f793c415c0008fca59a7c25f4ff7e049bc0b`를 유지한다. 로그는 `build/typed-styles-build.log`, `build/typed-styles-tests.log`, `build/typed-styles-consumer-build.log`, `build/typed-styles-consumer-tests.log`에 있다.

앱 고유 Color·Metric과 수작업 C++ 패널의 공유 방법은 [스타일 프로필](StyleProfiles.md)에 기록한다.
