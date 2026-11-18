# LVUI WebAssembly 대상

웹 대상은 호스트의 `lvui-compile`이 `.lvui`를 C++로 미리 변환하고, 생성된 뷰와
`LVUI::Native`, iiGUI 및 iiSDKCore를 Emscripten으로 연결한다. 같은 iiGUI 컴포넌트,
레이아웃, C++ 뷰모델과 변경 알림을 사용한다. 브라우저에는 단일 캔버스와 WebGL2
호스트를 제공하며 LVUI 파서나 DOM 위젯 트리는 배포하지 않는다.

## 계약

- 앱은 교차 빌드에서 호스트용 `LVUI_HOST_COMPILER`와 Emscripten용 LVUI, iiGUI,
  같은 실행 모드의 iiSDKCore 패키지를 명시한다. 네이티브 라이브러리를 WebAssembly
  링크에 혼합하지 않는다.
- `lvui_add_application`으로 생성한 대상에 `lvui_configure_web`을 적용한다. 앱이
  `SHELL_FILE`과 선택적인 `RESOURCES`를 제공하며, 리소스는 `/resources/`로
  사전 로드한다. `lvui_install_web`은 HTML과 대응하는 JS, WASM 및 리소스가 있는
  경우 데이터 파일을 같은 디렉터리에 설치한다.
- 셸은 `#canvas`와 Emscripten의 `{{{ SCRIPT }}}` 삽입점을 제공한다. CSS는 캔버스의
  논리 크기를 소유한다. 가변 크기를 원하면 창 선언의 고정 크기로 CSS를 덮어쓰지
  않도록 하고, 상위 요소에 명확한 크기를 지정한다.
- 예제 셸은 캔버스를 뷰포트에 맞추고 `data-iigui-accessibility`로 iiGUI 의미
  정보를 브라우저 접근성 피어에 연결한다. 자체 셸은 추가로
  `data-iigui-respect-minimum`을 지정해 최소 창 크기보다 작은 화면에서 브라우저
  스크롤을 사용할 수 있다. 두 정책은 컴포넌트의 그리기나 상태 소유권을 DOM에 넘기지 않는다.
- iiGUI는 캔버스 자체의 크기와 기기 배율 변경을 이벤트로 관찰하여 공통 위젯에
  배치를 다시 요청한다. 0 크기의 캔버스에서는 그리기를 중지하고 유효한 크기가
  돌아오면 다시 배치한다. 반응형 값은 변경 알림과 `EventListener.onSizeChanged`를
  통해 C++ 뷰모델에 연결할 수 있다.
- pthread 빌드는 SharedArrayBuffer, OffscreenCanvas/WebGL2와 worker의
  `requestAnimationFrame`, `Atomics.waitAsync`, Worker FontFace가 필요하고,
  서버에 COOP `same-origin`과 COEP `require-corp`가 필요하다. 단일 스레드에는
  이 요구 없이 WebGL2가 필요하다. `ResizeObserver`가 없으면 창과 배율만 관찰한다.
  입력·글꼴·접근성의 실제 범위는 [iiGUI 웹 호스트](../../iiGUI/docs/HostPlatforms.md#web-실행-계약)와
  [캔버스 크기 계약](../../iiGUI/docs/WebResponsive.md)을 따른다.

## 소스에서 예제 빌드

다음은 Workspace 체크아웃과 Emscripten, Ninja 및 이미 네이티브로 빌드한
`SDK/LVUI/build/lvui-compile`을 전제로 한다. 예제는 기존 C++ 뷰모델 카운터를
사용하며 별도의 뷰 디자인이나 DOM 컴포넌트를 만들지 않는다. 설치 접두사는 모두
각 저장소의 `build/` 아래이다.

```sh
ws=/Volumes/Storage/Workspace
emcmake cmake -S "$ws/SDK/iiSDKCore" -B "$ws/SDK/iiSDKCore/build/web" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DIISDKCORE_BUILD_EXAMPLES=OFF \
  -DIISDKCORE_WASM_THREADS=ON -DCMAKE_INSTALL_PREFIX="$ws/SDK/iiSDKCore/build/web-stage"
cmake --build "$ws/SDK/iiSDKCore/build/web" --parallel 4
cmake --install "$ws/SDK/iiSDKCore/build/web"
emcmake cmake -S "$ws/SDK/iiGUI" -B "$ws/SDK/iiGUI/build/web" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DIIGUI_BUILD_EXAMPLES=OFF \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DiiSDKCore_DIR="$ws/SDK/iiSDKCore/build/web-stage/lib/cmake/iiSDKCore" \
  -DCMAKE_INSTALL_PREFIX="$ws/SDK/iiGUI/build/web-stage"
cmake --build "$ws/SDK/iiGUI/build/web" --parallel 4
cmake --install "$ws/SDK/iiGUI/build/web"
emcmake cmake -S "$ws/SDK/LVUI" -B "$ws/SDK/LVUI/build/web" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DiiSDKCore_DIR="$ws/SDK/iiSDKCore/build/web-stage/lib/cmake/iiSDKCore" \
  -DCMAKE_INSTALL_PREFIX="$ws/SDK/LVUI/build/web-stage"
cmake --build "$ws/SDK/LVUI/build/web" --parallel 4
cmake --install "$ws/SDK/LVUI/build/web"
emcmake cmake -S "$ws/SDK/LVUI/example/Web" -B "$ws/SDK/LVUI/build/web-example" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DiiSDKCore_DIR="$ws/SDK/iiSDKCore/build/web-stage/lib/cmake/iiSDKCore" \
  -DiiGUI_DIR="$ws/SDK/iiGUI/build/web-stage/lib/cmake/iiGUI" \
  -DLVUI_DIR="$ws/SDK/LVUI/build/web-stage/lib/cmake/LVUI" \
  -DLVUI_HOST_COMPILER="$ws/SDK/LVUI/build/lvui-compile" \
  -DCMAKE_INSTALL_PREFIX="$ws/SDK/LVUI/build/web-package"
cmake --build "$ws/SDK/LVUI/build/web-example" --parallel 4
cmake --install "$ws/SDK/LVUI/build/web-example"
```

결과 디렉터리의 `LVUICounterWeb.html`, `.js`, `.wasm`, `.data`와 생성된 경우
`.worker.js`를 함께 제공한다. 예제 셸은 캔버스를 부모에 맞추기 위해 CSS 크기를
우선한다. 자체 앱은 셸과 크기 정책을 교체할 수 있다. 기본 글꼴 입력은
`SDK/iiGUI/resources/native`에서 가져오며, 다른 소스 경로는 예제의
`LVUI_WEB_IIGUI_RESOURCE_DIR`로 지정한다. Web 타깃의 Python 컴파일러 브리지는
기본으로 빌드하지 않는다. 호스트의 네이티브 Python API와 컴파일러는 유지한다.

같은 Web 예제 빌드와 설치 명령은 `LVUIReadingListWeb.html`,
`LVUIInventoryWeb.html`, `LVUIControlsFixtureWeb.html`,
`LVUIRadioRowsFixtureWeb.html`, `LVUIToggleFixtureWeb.html`,
`LVUISliderFixtureWeb.html`도 생성한다. 앞의 두 예제는 기존 GettingStarted의
`.lvui` 뷰와 C++ 뷰모델을 그대로 사용한다. 나머지는 기존 컨트롤 검증 fixture이며
일부는 색상 재정의 시험 항목을 포함한다. 각 HTML은 같은 이름의
JS·WASM·data·worker 파일과 함께 제공해야 한다.

웹의 편집용 `TextLine`과 IME는 아직 미구현이다. `InputField`가 포함된 읽기 목록과
복합 컨트롤 fixture는 현재 뷰 생성에 실패하는 재현 사례이며, 인벤토리 표의 셀도
편집할 수 없다. 미지원 편집 요청은 `unsupported`로 보고하고 표의 나머지 입력과
정렬은 계속할 수 있다. 라디오·체크박스·토글·슬라이더 fixture는 해당 편집 서비스에
의존하지 않는다.

다른 앱에서는 `find_package`로 두 대상 패키지를 찾고 `lvui_add_application` 다음에
`lvui_configure_web(MyApp SHELL_FILE <파일> RESOURCES <파일들>)`과
`lvui_install_web(MyApp DESTINATION <상대경로>)`을 호출한다. 같은 이름의 리소스는
충돌로 거부한다. 리소스가 없으면 `.data` 설치 항목을 만들지 않는다.

## 검증 경계

빌드·헤드리스 자동 테스트와 실제 브라우저의 입력·출력·시각 검증은 별개이다.
전체 컴포넌트의 브라우저별 동작이나 완전한 애플리케이션 기능을 위 계약만으로
보증하지 않는다.

2026-11-16 macOS arm64에서 현재 소스로 iiSDKCore·iiGUI·LVUI Web 정적 패키지를
교차 빌드하고, 설치 패키지 소비자가 카운터 뷰와 크기 콜백 fixture를 AOT C++로
변환해 WebAssembly에 컴파일·링크했다. HTML·JS·WASM·data·worker 파일의 설치도
확인했다. 네이티브 헤드리스 테스트와 JS 모의 관찰자는 실제 브라우저나 GPU 화면,
입력 및 모든 컴포넌트의 품질을 검증한 결과가 아니다.

## 로컬에서 제공하기

다음 명령은 필요한 격리 헤더를 붙여 loopback 주소에서만 파일을 제공한다.
서버의 기존 안내 문구는 HelloHosts를 가리키므로 실제 예제 주소는 아래와 같다.

```sh
python3 -B "$ws/SDK/iiGUI/tools/serve_web.py" "$ws/SDK/LVUI/build/web-package"
# http://127.0.0.1:8765/LVUICounterWeb.html
```

이 명령은 예제를 실행하거나 검증했다는 뜻이 아니며, 사용자가 직접 실행한다.

## 실제 브라우저 검증

2026-11-16 macOS arm64의 Chromium 157 기반 브라우저에서 위 카운터 예제를
실행했다. 1280×720과 390×844 뷰포트에서 `+`의 반복 입력, C++ 뷰모델 값과
레이블의 갱신, `Reset`, 크기 변경 후 상태 유지를 확인했다. 별도의 임베디드
fixture에서는 720×480과 320×480 캔버스 사이의 재배치와 0×0에서 복원한 뒤
입력 및 기존 값 유지를 확인했다. 포인터와 스페이스 키 활성화도 확인했으며,
검증한 두 페이지에서 브라우저 콘솔 경고와 오류는 관찰되지 않았다.

이 검증은 카운터 예제의 `Window`, `VStack`, `Label`, `PushButton`과 해당 입력
경로에 한정된다. 모든 LVUI 컴포넌트, 브라우저 엔진, 실제 모바일 기기 및 GPU
구현에 대한 검증은 아니다.

같은 날 Chromium 157 기반 브라우저에서 추가로 표의 행·열 렌더링과 머리글 정렬,
라디오 그룹의 상호 배타적 선택, 체크박스와 토글의 전환, 슬라이더 드래그와
방향키를 확인했다. 1280×720과 390×844 사이의 크기 변경 후 선택과 슬라이더
값이 유지되었다. 인벤토리 예제의 390px 화면에서 오른쪽 열이 계속 잘린다는
초기 판단은 렌더링이 안정되기 전의 관찰이었다. 다시 확인한 결과 표는 사용 가능한
폭으로 줄어들며 방향키로 마지막 열을 선택하면 내부 스크롤이 열 전체를 보인다.
1280px로 넓힌 뒤에도 선택은 유지되었다.

셀 편집 진입은 웹 텍스트 배치가 없어 지원되지 않으며, 오류 처리 수정 후에는
편집 요청 뒤에도 정렬과 키보드 선택을 계속할 수 있음을 확인했다. 읽기 목록과
복합 컨트롤 fixture는 같은 미지원 입력 필드 때문에 아직 브라우저에서 열리지
않는다.

선택형 최소 크기 정책을 적용한 별도 Chromium 검증에서는 390px 뷰포트에서
브라우저 가로 스크롤로 인벤토리의 `Status` 열에 도달해 정렬했다. 이 정책은
고정 최소 폭을 지키는 대안이며 자동 재배치를 뜻하지 않는다. 같은 브라우저의
접근성 트리에서 카운터의 버튼과 라디오·체크박스의 역할 및 상태를 확인하고
Tab, Space, 방향키로 iiGUI 뷰모델과 화면이 함께 갱신됨을 확인했다. 스위치의
전환과 슬라이더의 0–10 범위 및 Home·End 입력도 브라우저 의미 트리와 캔버스에서
확인했다. 임베디드 실험에서는 캔버스를 숨기면 피어가 제거되고 복원하면 이전
카운터 값과 입력이 유지되었다. 라디오 그룹 관계와 텍스트 입력·표 접근성은 아직
웹 피어에 연결하지 않았다. 실제 스크린리더나 다른 브라우저 엔진은 검증하지 않았다.

## LVUI 문법으로 웹 앱 작성

`example/Web/Declarative`는 기존 카운터의 컴포넌트 배치를 재사용한다. `Main.lvui`의
`Application`과 `ApplicationWindow`에서 `Label`, `PushButton`을 선언하고,
`WebCounterModel.h`의 iiSDKCore `Observable`과 명령에 연결한다. 런타임 파싱이나
별도의 DOM 위젯 없이 LVUI가 생성한 C++ 뷰를 iiGUI 캔버스에 표시한다. 예제와
기본 웹 셸은 LVUI 설치 패키지에도 포함된다.

소비자 CMake에서는 Web용 iiSDKCore·iiGUI·LVUI 패키지를 찾고 다음과 같이 앱을
정의한다. `INCLUDES`의 헤더는 소비자 소스 디렉터리에서 찾는다. 다른 디렉터리의
헤더는 해당 대상에 `target_include_directories`로 추가한다.

```cmake
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI 0.6 CONFIG REQUIRED)
lvui_add_web_application(MyApp
    SOURCE Main.lvui
    INCLUDES WebCounterModel.h
    INSTALL_DESTINATION .)
```

기본 셸은 설치된 LVUI에서, 기본 글꼴과 라이선스는 설치된 iiGUI의
`iiGUI_NATIVE_RESOURCES`에서 사용한다. `SHELL_FILE`로 셸을 교체하고,
`RESOURCES`로 추가 파일을 `/resources/`에 사전 로드할 수 있다.
`NO_DEFAULT_FONTS`는 iiGUI 기본 리소스의 자동 포함을 끈다. 같은 파일 이름의
리소스는 충돌로 거부한다. 기본 부트스트랩은 `Application` 루트를 대상으로 하며,
별도의 초기화나 재사용 `Window` 루트에는 기존 `CONFIGURE_SOURCE`를 지정한다.
`NAME`, `STYLE_FILE`, `STYLE_MODES`, `SOURCES`, `LIBRARIES`, `OUTPUT_VARIABLE`도
기존 `lvui_add_application`과 같은 뜻이다. 설치는 `INSTALL_DESTINATION`을
지정했을 때만 설정하며 생략하면 나중에 `lvui_install_web`을 호출할 수 있다.

앞 절에서 Web용 패키지를 설치한 뒤, 같은 `ws` 변수를 사용해 예제를 빌드한다.
소스 체크아웃 대신 설치된 `share/LVUI/examples/Web/Declarative` 디렉터리를
별도 프로젝트로 복사해도 된다.

```sh
emcmake cmake -S "$ws/SDK/LVUI/example/Web/Declarative" -B "$ws/SDK/LVUI/build/declarative-web" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DCMAKE_PREFIX_PATH="$ws/SDK/LVUI/build/web-stage;$ws/SDK/iiGUI/build/web-stage;$ws/SDK/iiSDKCore/build/web-stage" \
  -DLVUI_HOST_COMPILER="$ws/SDK/LVUI/build/lvui-compile" \
  -DCMAKE_INSTALL_PREFIX="$ws/SDK/LVUI/build/declarative-web-package"
cmake --build "$ws/SDK/LVUI/build/declarative-web" --parallel 4
cmake --install "$ws/SDK/LVUI/build/declarative-web"
python3 -B "$ws/SDK/iiGUI/tools/serve_web.py" "$ws/SDK/LVUI/build/declarative-web-package"
# http://127.0.0.1:8765/LVUIDeclarativeWeb.html
```

앞서 설명한 브라우저 격리·스레드·WebGL 요구사항과 텍스트 편집 제한은 이
작성 경로에도 그대로 적용된다.

2026-11-16 macOS arm64에서 설치된 LVUI 예제를 별도 소비자 디렉터리로 복사하고
설치된 Web용 LVUI·iiGUI·iiSDKCore와 iiGUI 의존 패키지로 AOT 빌드 및 설치했다. Chromium
기반 브라우저의 1280×720과 390×844 화면에서 `+`, `Reset`, Tab·Space 입력과
뷰모델 값의 화면 갱신을 확인했다. 크기를 바꾸어도 값은 유지되었고 해당 실행에서
콘솔 경고와 오류는 관찰되지 않았다. 이 결과는 선언형 카운터와 이 브라우저에
한정되며 전체 컴포넌트나 다른 브라우저의 동작을 보증하지 않는다.

Core의 `IISDKCORE_WASM_THREADS=OFF` 설정으로 단일 스레드 Core를 별도 빌드하고
같은 패키지로 iiGUI와 LVUI를 빌드할 수 있다. 위 명령의 `ON` 예는 격리가 필요한
pthread 모드다. 두 모드는 별개의 산출물이다. Emscripten용 LVUI는 C++ 모듈
스캐너를 기본 요구하지 않으며 `CMAKE_CXX_SCAN_FOR_MODULES` 명시값은 존중한다.

설치된 LVUI가 Core Observation을 확인하는 C++ 헤더 검사에도 모듈 스캐너는
필요하지 않다. 검사의 로컬 설정은 앱의 C++ 모듈 설정을 변경하지 않는다.

## 브라우저 카운터 예제

`example/Web/BrowserDemo`는 LVUI 선언과 C++ `BrowserDemoModel`을 사용하는
작은 카운터다. 기본 버튼으로 값을 늘리거나 초기화한다. 단일 스레드 Core와
이에 맞춘 iiGUI·LVUI 패키지 및 호스트 `lvui-compile`로 Emscripten 교차 빌드한다.
생성된 HTML·JS·Wasm·데이터 파일은 같은 디렉터리에서 HTTP로 제공한다.
디자인은 [LVUI 카운터 시안](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI?node-id=1966-14)을 따른다.
