# LVUI 기본 스타일 토큰

LVUI는 [LVRS 원본 Figma](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVRS)의 변수를 기본 스타일 카탈로그로 제공한다. 원본 변수 이름, Figma ID/key, 컬렉션, 모드, 타입, 설명, scope, 게시 숨김 여부, CSS 이름과 별칭을 보존한다. 빌드나 앱 실행 중 Figma 접속은 필요하지 않다.

이 스냅샷은 LVUI가 소유하는 시각값이다. 기본 코드 생성 경로는 값으로 `WidgetAppearance`를
구성하며 `ii::gui::lvrs_appearance()`를 호출하지 않는다. 이전 `FRAMEWORK LVRS`
생성 경로는 제거되었다. 기능 이식과 독립 패키지의 현재 검증 상태는
[독립 구현 현황](LVUIIndependence.md)을 따른다.

색상과 수치는 [Color·Metric 값 타입](StyleTypes.md)으로 선택한다. `Color.primary`, `Metric.space8` 또는 문맥상의 `.primary`, `.space8`을 사용할 수 있다. [전체 멤버 목록](StyleTokenMembers.md)은 원본 스냅샷에서 자동 생성한다. 아래 `token()`·`var()` 형식도 같은 값을 참조하는 호환 API이다.

## 원본과 범위

2026-10-25에 읽은 **208개 변수, 5개 컬렉션**을 LVUI가 소유하는 `catalog/lvui-style-tokens.json`에 저장했다. 기존 시각 언어의 출처와 호환 식별자는 데이터에 보존한다. `tools/update_style_tokens.py`가 이를 검증하여 `src/Style/StyleTokens.generated.inc`를 생성한다. 일반 빌드는 체크인된 카탈로그를 사용하며 Python도 필요하지 않다.

| 컬렉션 | 변수 수 | 기본 모드 |
|---|---:|---|
| Theme Colors | 115 | Dark |
| Material | 20 | Dark |
| ColorPicker | 5 | Dark |
| Theme Typography | 20 | Value |
| Theme Metrics | 48 | Dark |

타입은 Color 138개, Number 67개, String 3개이다. 별칭 변수 6개를 값으로 펼치지 않고 보존한다. `Action/primary`와 `ColorPicker/Previous color` 같은 연쇄 별칭도 해석한다. 공개 API는 이후 Boolean 변수도 받을 수 있다. 현재 원본에 Light 모드는 없으며 존재하지 않는 모드 요청은 오류이다.

## 기본값

옵션 없이 사용하는 `parse()`와 `compile_native()`에 카탈로그가 연결된다. 명시적인 컴포넌트 입력과 인라인 스타일이 기본값보다 우선한다.

| 대상 | 기본 토큰 |
|---|---|
| 파서 루트 / AOT 창 배경 | Theme Colors / Panel/window |
| 파서 기본 텍스트 | Text/bodyColor, Body/textBody, Weight/Medium |
| 파서 h1 | Title/textTitle, Weight/Bold |
| AOT 글꼴 | Theme Typography / Font/fontBody = Pretendard |
| AOT 스택 간격·패딩 | Theme Metrics / Space/8, Space/0 |
| AOT 버튼 높이·수평 패딩·반경 | Size/22, Space/8, Radius/8 |
| AOT 기본 액션·상태 색상 | Action/primary, Accent/Blue/Hover, Accent/Blue/Pressed |
| AOT 비활성 배경 | Panel/subSurface |

AOT Label의 기본 역할은 기존 description을 유지한다. 크기·줄 높이·굵기·색상은 다음 토큰 조합으로 결정한다.

| 역할 | 크기 / 줄 높이 | 굵기 | 색상 |
|---|---:|---:|---|
| title | 26 / 26 | 700 | titleHeaderColor |
| title2 | 22 / 22 | 700 | titleHeaderColor |
| header | 17 / 17 | 600 | titleHeaderColor |
| header2 | 15 / 15 | 600 | titleHeaderColor |
| body | 13 / 13 | 500 | bodyColor |
| description | 12 / 12 | 600 | descriptionColor |
| caption / disabled | 11 / 11 | 400 | captionColor / disabledColor |

이 표는 기본 스냅샷 값이며 앱별 설정으로 바뀔 수 있다. `fontSize`만 지정하면 해당 크기를 줄 높이로 사용하고, `lineHeight: 0`은 네이티브 글꼴 측정 규칙을 사용한다. 창 배경은 Panel/window를 사용한다. 창의 `accent`는 LVUI 자체 색상 선택 타입으로 기본 호스트와 기본 버튼에 적용하며, 상세 범위는 [Window 계약](NativeWindow.md)을 따른다.

현재 네이티브 글꼴이 지원하는 굵기는 400/500/600/700이다. 다른 굵기는 코드 생성이나 초기 파서 단계에서 진단을 반환한다.

다른 컴포넌트도 공개 입력에 `token()`을 전달하여 같은 카탈로그를 사용할 수 있다. 모든 LVRS 컴포넌트의 개별 내부 상태나 효과를 이번 작업에서 다시 구현한 것은 아니다. Tracking/Normal을 비롯한 전체 변수는 조회 가능하며, 해당 값을 받는 컴포넌트의 지원 입력과 타입·범위 검증을 따른다.

## 인라인 스타일과 컴포넌트 문법

원본 `codeSyntax.WEB`의 CSS 이름을 그대로 사용한다.

```html
<ApplicationWindow title="LVUI"
    style="background:var(--lvrs-theme-colors-panel-window);font-family:var(--lvrs-theme-typography-font-font-body)">
  <div style="padding:var(--lvrs-theme-metrics-space-24);gap:var(--lvrs-theme-metrics-space-8)">
    <span style="font-size:var(--lvrs-theme-typography-body-text-body);font-weight:var(--lvrs-theme-typography-weight-medium);color:var(--lvrs-theme-colors-text-body-color)">Native LVUI</span>
  </div>
</ApplicationWindow>
```

네이티브 마크업의 `background`/`background-color`, `border-color`, `border-width`, `border-radius`는 스택의 네이티브 그리기에 연결된다. `font-family`는 창의 appearance 입력이다. 일반 스택에 배경과 보더를 자동으로 추가하지 않으며, `border-width`를 지정할 때는 `border-color`가 필요하다. 속성을 받지 않는 컴포넌트에 지정하면 진단을 반환한다.

`var(--missing, 8px)`와 중첩 fallback을 지원한다. fallback은 이름이 없을 때만 사용하며 잘못된 타입·설정·순환을 숨기지 않는다. 토큰 표현은 전체 속성 값이어야 한다. CSS cascade, 사용자 정의 변수 선언, `calc()`와 문자열 보간은 제공하지 않는다. Number와 Metric은 길이 입력에서 논리 픽셀로 해석한다.

초기 HTML 파서 `parse("<main>…</main>")`도 기존 지원 속성에서 같은 토큰을 해석한다. 기존 `Style` 구조체와 렌더링 ABI는 유지한다. 네이티브 AOT 마크업의 모든 속성을 초기 파서에도 추가한 것은 아니다.

```text
let ink = token("Theme Colors/Text/bodyColor")
ApplicationWindow {
    VStack(spacing: token("--lvrs-theme-metrics-space-8")) {
        Label(text: "LVRS defaults", style: .title)
        Label(text: "Native", color: ink,
              fontSize: token("Theme Typography/Body/textBody"))
    }
}
```

`token()`은 빌드 시점에 이름 하나를 받는다. CSS 이름, `컬렉션/변수명`, Figma ID/key로 조회한다. 단독 변수명은 유일할 때만 사용할 수 있다. 장기 사용에는 CSS 이름 또는 ID를 권장한다. Color도 불변 `let`에 보관할 수 있으며 현재 변경 가능한 Color `var`는 지원하지 않는다. 기존 Number/String/Bool의 반응형 상태 기능은 유지한다.

생성 헤더에는 확정된 스타일 C++ 숫자·문자열·WidgetColor가 들어간다. 실행 중에는
토큰 카탈로그 조회, Figma API와 소스 인터프리터가 필요하지 않다. 독립 컴포넌트는
필요한 LVUI 자체 헤더를 생성 코드에서 포함할 수 있다.

## C++ API와 앱별 설정

```cpp
#include <LVUI/Compiler.h>
#include <LVUI/StyleTokens.h>

const auto& catalog = lvui::default_style_tokens();
auto spacing = catalog.number("--lvrs-theme-metrics-space-8");
auto edge = catalog.color("--lvrs-material-surface-dense-edge");
auto font = catalog.string("Theme Typography/Font/fontBody");

lvui::NativeCompileOptions options;
options.style_tokens.modes["Theme Colors"] = "Dark";
options.style_tokens.overrides["--lvrs-theme-metrics-space-8"] = 10.0;
options.style_tokens.overrides["--lvrs-theme-colors-action-primary"] =
    lvui::StyleColor{0.2, 0.4, 0.8, 1.0};
auto result = lvui::compile_native(source, options);
```

`collections()`와 `tokens()`는 읽기 전용 span을 제공하며 `find()`로 메타데이터를 조회한다. `resolve()`는 `std::expected<StyleTokenValue, StyleTokenError>`를 반환한다. `number()`, `color()`, `string()`, `boolean()`은 타입을 강제 변환하지 않는다. `validate()`는 선택한 모드에서 전체 카탈로그와 설정을 검사한다.

`StyleTokenOptions`는 호출별 값이다. 기본 카탈로그를 변경하지 않으므로 앱·문서·컴파일 사이에 값이 유출되지 않는다. 별칭 대상을 바꾸면 그 대상을 참조하는 별칭도 새 값을 받고, 별칭 자체를 바꾸면 그 지점부터 새 값을 사용한다. 같은 토큰을 CSS 이름과 ID 등으로 중복 지정하면 오류이다.

컬렉션과 모드는 이름 또는 ID로 선택한다. 알 수 없는 이름, 충돌 모드, 타입 불일치, 누락 별칭과 순환은 오류이다. 별칭 깊이는 최대 64개 토큰이다. Number는 유한값, RGBA는 채널별 0..1, String은 유효한 UTF-8·NUL 없음·4096바이트 이하를 요구한다. 글자 크기·길이 등 의미상의 범위는 사용하는 속성에서 추가 검사한다.

```sh
build/lvui-compile \
  --style-mode 'Theme Colors=Dark' \
  --style-mode 'Theme Typography=Value' example/StyleTokens/App/TokenWindow.lvui
```

CMake는 `lvui_generate(... STYLE_MODES "Theme Colors=Dark" "Theme Typography=Value")`를 지원한다. 앱별 값 변경은 C++ `NativeCompileOptions::style_tokens.overrides`로 전달한다. 이 설정은 코드 생성 시 확정되며 실행 중 테마 전환 API는 아니다.

## 색 공간·알파·단위

`StyleColor`는 Figma의 encoded sRGB와 독립 알파를 double로 보존한다. 8비트 HEX로 양자화하거나 RGB에 알파를 미리 곱하지 않는다. 초기 HTML 트리의 `Color`는 기존 float 계약을 유지한다. WidgetColor는 encoded sRGB, 저수준 WidgetDrawing은 linear RGB를 받으므로 생성 스택 그리기 코드가 경계에서 한 번 변환한다.

`--lvrs-material-surface-dense-edge`는 흰색과 약 0.12의 알파이다. 이를 GPU의 linear blending에 전달하는 것과 Figma의 encoded sRGB 합성색을 사용하는 것은 구분해야 한다. 배경이 확정되어 평면 합성색이 필요할 때 명시적으로 다음 함수를 사용한다.

```text
let edge = composite(token("--lvrs-material-surface-dense-edge"),
                     token("--lvrs-theme-colors-panel-window"))
ApplicationWindow {
    VStack(borderColor: edge,
           borderWidth: token("--lvrs-theme-metrics-stroke-1")) {
        Label(text: "Figma sRGB composite")
    }
}
```

C++에서는 `composite_srgb(foreground, background)`를 사용한다. 투명 배경도 straight-alpha로 계산한다. 원본 토큰은 바꾸지 않으며 배경을 추측하여 모든 색상에 적용하지 않는다. 기본 창 배경 위의 위 보더는 8비트 표시값 약 48이다.

Number는 원본 수치를 유지한다. Opacity/32는 **32**이며 0.32로 바뀌지 않는다. 퍼센트와 논리 픽셀 처리는 사용하는 속성의 계약을 따른다. ColorPicker/Hex와 ColorPicker/Opacity도 원본 String 타입을 유지한다.

## 원본 갱신과 검증

Congregation의 로컬 복사본이 아닌 위 LVRS 원본에서 Figma Plugin API `getLocalVariableCollectionsAsync()`와 `getLocalVariablesAsync()`로 읽는다. 다음 항목을 스냅샷에 보존한다.

- 컬렉션: id, key, name, modes, defaultModeId, hiddenFromPublishing, 변수 count.
- 변수: id, key, name, variableCollectionId를 옮긴 collectionId, resolvedType을 옮긴 type, valuesByMode, scopes, hiddenFromPublishing, codeSyntax, description.
- source: provider, fileKey, capturedOn. 최상위 schema는 1이다.

도구의 응답 크기가 제한되면 ID순 변수 목록을 64개 정도로 나누어 읽고 합친다. 모든 컬렉션 count와 전체 합계가 일치해야 한다. `VARIABLE_ALIAS`와 대상 ID를 그대로 저장한다. LVUI의 디자인을 변경하기로 결정한 경우에만 값을 갱신한다. 레거시 LVRS의 변경을 자동으로 추종하지 않는다. 갱신 뒤 다음 명령을 실행한다.

```sh
python3 -B tools/update_style_tokens.py
python3 -B tools/update_style_tokens.py --check
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

생성기는 누락된 컬렉션·모드·별칭, 타입·RGBA 오류, CSS 이름 충돌과 순환을 거부한다. `--check`는 파일을 수정하지 않고 생성 코드의 드리프트를 검사한다. 테스트 실행에는 Python이 필요하지만 일반 빌드에는 필요하지 않다.

`tests/style_tokens.cpp`는 전체 수량·타입·조회, 별칭, 다중 모드 카탈로그, 격리된 값 변경, CSS fallback, 색상 합성, 파서 기본값, AOT 적용과 오류를 검증한다. Python 테스트는 생성기 재현성·잘못된 스냅샷·UTF-8 리터럴을 검사한다. 기존 LVUI 테스트도 함께 실행한다.

[`example/StyleTokens`](../example/StyleTokens/README.md)는 LVUI와 별도로 공급하는 iiGUI 호스트를 사용하여 카탈로그 소비자와 두 종류의 AOT 뷰를 빌드한다. LVRS는 찾거나 링크하지 않는다. 선택적 macOS 네이티브 테스트는 Metal/Vulkan에서 생성된 화면을 띄우고 창 배경·합성 보더 픽셀·Pretendard 글자 크기를 검사한다. 빌드는 저장소 `build/` 아래에 둔다.

### 2026-10-25 토큰 초기 구현 검증 기록

macOS arm64에서 Release 빌드와 LVUI 전체 CTest **56/56**이 통과했다. `build/style-token-prefix`에 설치한 패키지를 별도 소비자가 사용하도록 구성했고, Qt5/Qt6 패키지 탐색을 비활성화한 상태에서 두 문법의 AOT 헤더와 소비자 실행 파일을 빌드했다. 소비자 CTest **4/4**도 통과했다.

Metal과 Vulkan 모두 실제 GPU와 sRGB attachment를 확인했다. 생성된 LVRS 창에서 배경의 8비트 값 20, 기본 배경 위 sRGB 보더 합성값 약 48, Pretendard 제목 26·본문 13을 검사했다. 생성 헤더에 LVUI 카탈로그 호출이나 소스 파서가 없고, 소비자 및 연결된 Core/FileProvider 라이브러리에 Qt 링크가 없는 것도 확인했다. Windows/Linux의 실제 실행은 이 기록에 포함하지 않는다.

로그는 `build/style-tokens-build.log`, `build/style-tokens-tests.log`, `build/style-tokens-consumer-build.log`, `build/style-tokens-consumer-tests.log`, `build/style-tokens-dependencies.log`에 있다. 원본 스냅샷 SHA-256은 `7d9b1bfa1c345568afe4fea6ba97f793c415c0008fca59a7c25f4ff7e049bc0b`이다.
