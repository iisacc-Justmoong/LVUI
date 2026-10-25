# 앱 공통 Color·Metric 프로필

앱의 LVUI 화면과 수작업 C++ 패널은 하나의 타입 지정 프로필을 공유할 수 있다. 프로필은 빌드 입력이며 실행 중 파일을 읽거나 스타일 토큰을 해석하지 않는다. 기존 LVRS 기본값을 사용하면서 앱 고유 색상·배치 치수만 추가한다.

```text
// Theme.lvui
let window: Color = Color("#141719")
let inspectorWidth: Metric = 352
let edge: Color = composite(.material.surface.dense.edge, .window)
```

`let 이름: Color/Metric = 값`만 허용한다. import, var, UI 트리와 다른 타입은 거부한다. 앞서 선언한 이름은 뒤 선언에서 사용할 수 있다. 기본 멤버 이름은 해당 앱의 값을 변경하고, 새 이름은 `Color.이름` 또는 `Metric.이름`을 추가한다. 기본 카탈로그와 다른 컴파일의 값은 바뀌지 않는다. 기존 값 변경을 ID·CSS·경로로 지정했더라도 같은 기본 멤버를 프로필에서 다시 지정하면 해당 설정을 대체한다. 잘못된 기본 옵션은 적용 전에 거부한다.

```html
<ApplicationWindow backgroundColor={Color.window}>
  <div style="width:Metric.inspectorWidth;gap:Metric.space8">
    <Label text="Inspector" color={Color.text.body}/>
  </div>
</ApplicationWindow>
```

## CMake 연결

```cmake
set(theme "${CMAKE_CURRENT_SOURCE_DIR}/App/views/Theme.lvui")
lvui_generate(NAME Main SOURCE App/views/Main.lvui FRAMEWORK LVRS
    STYLE_FILE "${theme}" OUTPUT_VARIABLE main_header)
lvui_generate_styles(NAME AppStyle SOURCE "${theme}" NAMESPACE app.style
    OUTPUT_VARIABLE style_header)
add_custom_target(AppStyleHeader DEPENDS "${style_header}")
add_dependencies(MyApp AppStyleHeader)
target_include_directories(MyApp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
```

프로필 파일은 두 생성 명령의 의존성이다. 파일을 수정하면 뷰와 공통 헤더를 다시 생성하고, 잘못된 입력은 이전 생성 결과를 덮어쓰지 않는다. 기본 모드는 카탈로그를 따르며 두 함수 모두 `STYLE_MODES "Collection=Mode"`를 받을 수 있다. 같은 앱의 두 생성 경로에는 동일한 모드를 지정해야 한다. 교차 컴파일 시 두 함수 모두 호스트용 `LVUI_HOST_COMPILER`가 필요하다.

```cpp
#include "AppStyle.generated.h"
const auto background = app::style::Color::window;
const auto text = app::style::Color::textBody;
const auto width = app::style::Metric::inspectorWidth;
```

생성 헤더의 Color 값은 encoded sRGB `ii::gui::WidgetColor`이며, LVUI의 점으로 구분한 멤버를 C++의 lowerCamelCase로 합친다. `Color.material.surface.dense.edge`는 `Color::materialSurfaceDenseEdge`이다. 앱 멤버와 합친 이름이 충돌하거나 C++ 예약어이면 헤더 생성을 거부한다. 생성물은 `iiGUI/Appearance.h`만 포함하며 LVUI 라이브러리·파서·카탈로그에 의존하지 않는다.

C++ Metric은 정수인 경우 정수 상수, 소수인 경우 네이티브 `Rect`에 직접 전달할 수 있는 float 상수로 생성한다. LVUI 뷰 컴파일의 수치 해석은 기존 double을 유지한다. 브러시 크기·문서 해상도·픽셀 데이터 같은 문서 모델 값은 이 스타일 프로필로 옮기지 않는다.

## 도구와 공개 API

```sh
lvui-compile --style-file Theme.lvui --framework LVRS Main.lvui
lvui-compile --emit-styles --namespace app.style Theme.lvui
```

`compile_style_profile(source, base)`는 검증된 `StyleTokenOptions`를 반환한다. `compile_style_header(options, namespace)`는 같은 값으로 네이티브 헤더를 반환한다. 앱 전용 멤버는 `StyleTokenOptions::members`, 기존 토큰 변경은 `overrides`에서 유지한다. 프로필 크기는 1 MiB 이하이며 파일 읽기·타입·이름·색상 범위 오류는 표준 오류로 진단하고 부분 C++ 코드를 출력하지 않는다.

Color 생성자는 HEX/Color 값 하나 외에 `Color(r, g, b, a)`도 받는다. 네 채널은 유한한 0~1 범위의 encoded sRGB·독립 알파이다. Figma에서 가져온 소수 채널을 HEX 양자화 없이 보존할 때 사용할 수 있다.

## 2026-10-25 검증

macOS arm64 Release 빌드와 전체 LVUI CTest **61/61**이 통과했다(30.95초). 프로필의 타입·RGBA 범위·독립된 기본 옵션·이름 충돌·정수 상수 생성을 검사했다. 도구 테스트는 11개이며, 같은 프로필의 뷰/헤더 생성·값 변경 후 재생성·파일명 공백·잘못된 입력 시 이전 산출물 보존을 포함한다. 로그는 `build/app-style-profile-build.log`, `build/app-style-profile-tests.log`, 결과 XML은 `build/app-style-profile-tests.xml`이다.
