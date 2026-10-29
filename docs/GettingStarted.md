# LVUI 시작하기와 현재 사용 범위

이 문서는 2026-10-29의 LVUI 작업본과 함께 제공되는 세 개의 데스크톱 예제를
기준으로 한다. LVUI는 `.lvui` 선언을 빌드할 때 C++23으로 변환하는 AOT UI 언어다.
실행 중 선언 파일을 해석하지 않는다. LVUI가 지원하는 자체 컨트롤의 상태·그리기를
소유하고, iiGUI가 운영체제 창과 범용 입력·표시 서비스를 제공하며, iiSDKCore가
애플리케이션의 진입점과 수명을 소유한다.

```text
.lvui + 선택적 C++ 백엔드
       │ lvui_generate / lvui-compile (호스트에서 실행)
       ▼
생성된 .generated.h + 앱 구성 C++23
       │ 앱 빌드
       ▼
LVUI 네이티브 바인딩 + iiGUI 호스트 + iiSDKCore 런타임
```

현재 언어 레지스트리는 시각 이름 109개를 포함한다. 이름이 등록되었다는 사실만으로
모든 속성·운영체제·접근성이 완성되었다고 볼 수는 없다. 구현된 컨트롤과 전환 중인
경로는 [독립 구현 현황](LVUIIndependence.md), 언어 기능은
[문서 색인](README.md)을 확인한다. 기본 경로의 대상은 LVUI 자체 컨트롤이고,
`FRAMEWORK LVRS`는 더 이상 지원하지 않으며 명시적으로 선택하면 오류를 반환한다.

| 범위 | 사용 가능한 주요 접점 | 먼저 확인할 계약 |
|---|---|---|
| 화면 구성 | 창, 스택, 레이블, 조건부 View, 타입 입력과 슬롯, 키 기반 반복 | [배치](NativeLayout.md), [재사용 View](Components.md), [반복](Repetition.md) |
| 입력·기본 제어 | 버튼, 입력 필드, 스위치, 체크·라디오, 슬라이더, 스테퍼 | [버튼](NativeButton.md), [입력](NativeInputField.md), [스위치](NativeToggleSwitch.md) |
| 데이터 화면 | 가상 표, 일반 목록, 계층 목록·패널, 코드 편집기 | [표](NativeOwnedTable.md), [목록](NativeOwnedListView.md), [계층](NativeOwnedHierarchyList.md), [코드 편집기](NativeCodeEditor.md) |
| 오버레이·탐색 | 메뉴, 팝오버, 모달·시트, 경고, 툴팁, 페이지 라우터 | [메뉴](NativeMenu.md), [모달](NativeModal.md), [페이지 라우터](NativePageRouter.md) |
| 시각·모델 | 스타일 값, 진행률·모션, 이미지, 색상 선택과 공급자 | [스타일](StyleTokens.md), [진행률](NativeProgress.md), [이미지](NativeImage.md), [색상 선택](ColorSelection.md) |

각 계약 문서의 미완료 입력과 플랫폼 제한을 개별적으로 확인한다. 특히 동적 메뉴 자식,
표의 셀별 접근성과 실제 장치별 그리기 등은 위 목록이 완성을 뜻하지 않는다.

## 예제 둘러보기

| 앱 | 파일 | 익히는 내용 |
|---|---|---|
| Hello | [Hello.lvui](../example/GettingStarted/App/Hello.lvui) | 창, 수직 배치, 텍스트 역할, Metric·Color, 진행 표시 |
| ReadingList | [ReadingList.lvui](../example/GettingStarted/App/ReadingList.lvui), [백엔드](../example/GettingStarted/App/ReadingListBackend.h) | 텍스트 입력, Observable, 콜백, 재사용 View, 조건 분기와 키 기반 반복 |
| Inventory | [Inventory.lvui](../example/GettingStarted/App/Inventory.lvui), [백엔드](../example/GettingStarted/App/InventoryBackend.h) | 표 모델·편집 경계, 선택, 정렬, 클립보드, 변경 리비전 |

세 앱은 같은 [애플리케이션 연결 코드](../example/GettingStarted/App/Application.cpp)를
사용한다. 메모리에서만 데이터를 다루므로 창을 닫으면 목록과 표의 변경은 사라진다.
앱을 실제 제품에 넣을 때는 해당 모델을 파일·서비스 등 앱 소유 저장소로 교체한다.

## 준비와 빌드

C++23 컴파일러, CMake 3.31 이상, LVUI와 호환되는 iiSDKCore·iiColorManager·iiGUI
패키지가 필요하다. iiGUI의 데스크톱 빌드와 해당 플랫폼 그래픽 의존성을 준비한다.
예제는 macOS, Windows, Linux 데스크톱 호스트를 대상으로 구성하지만, 플랫폼별 실제
표시·입력 검증 상태는 각각 별개다. LVUI 저장소 루트에서 `build/`를 사용한다.

```sh
cmake -S . -B build -DLVUI_BUILD_TUTORIAL_EXAMPLES=ON -DBUILD_TESTING=ON \
  -DiiGUI_DIR=/path/to/iiGUI/lib/cmake/iiGUI \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DiiColorManager_DIR=/path/to/iiColorManager/lib/cmake/iiColorManager
cmake --build build --target LVUIHello LVUIReadingList LVUIInventory
```

위 경로는 설치 환경에 맞게 바꾼다. 세 앱은 `build/example/GettingStarted/`에
생성된다. macOS에서는 `LVUIHello.app`, `LVUIReadingList.app`, `LVUIInventory.app`을
열고, 다른 데스크톱에서는 같은 이름의 실행 파일을 사용한다. 빌드 시 글꼴·고지는
iiGUI 패키지의 원본 리소스 디렉터리에 연결한다. 배포할 때는 링크가 가리키는 파일과
라이선스를 배포 패키지에 포함해야 한다. 플랫폼에 따라 개발용 심볼릭 링크 생성 권한이
필요할 수 있다.

헤드리스 통합 테스트는 인접 `SDK/iiGUI/src/iiGUI/WidgetState.h`를 사용한다. 다른
위치에 있다면 `-DLVUI_TUTORIAL_IIGUI_SOURCE=/path/to/iiGUI/src`를 지정한다.
`BUILD_TESTING=OFF`이거나 해당 소스가 없으면 앱은 빌드되지만 테스트 대상이 등록되지
않는다. 이 검사는 OS 창,
GPU, 실제 입력기 또는 스크린 리더의 검증을 대체하지 않는다.

```sh
cmake --build build --target LVUITutorialTests
ctest --test-dir build --output-on-failure -R '^LVUI\.Tutorial\.Generated$'
```

2026-10-29에 조사한 `~/.local/SDK`의 LVUI 및 iiGUI 설치본은 이 작업본보다 오래되었다.
특히 기존 iiGUI 패키지 설정은 LVUI를 의존성으로 다시 찾는다. 최신 소스를 사용할
때는 호환되는 최신 iiGUI·Core 패키지를 선택하고 설치본을 소스와 같은 버전이라고
가정하지 않는다. 이 문서의 예제 추가가 설치본을 갱신하지는 않는다.

## 소스에서 화면 만들기

`ApplicationWindow` 안에 `VStack`·`HStack`과 컨트롤을 배치한다. 문자열, 숫자,
불리언과 스타일 역할을 이름 있는 입력으로 전달한다. ID는 앱이나 테스트가 생성된
위젯을 찾을 때 사용한다. Hello의 `Color.window`와 `Metric.space12`는 생성 시점에
선택한 LVUI 스타일 값으로 컴파일되며 실행 중 Figma 접속이 필요하지 않다.

```lvui
ApplicationWindow(title: "Hello", width: 640, height: 400) {
    VStack(spacing: 12, padding: 24) {
        Label(text: "Hello, LVUI", style: .title)
        ProgressBar(currentValue: 65, motionEnabled: false)
    }
}
```

CMake에서 `lvui_generate(NAME Hello SOURCE App/Hello.lvui OUTPUT_VARIABLE generated)`를
호출하면 빌드 디렉터리에 `Hello.generated.h`가 생긴다. 앱의 소스와 생성 파일을
`iisdkcore_add_application`에 전달하고 `LVUI::Native`, `iiGUI::iiGUI`를 링크한다.
`configure_application`은 생성된 `lvui_generated::Hello`의 창 옵션으로 iiGUI 호스트를
붙이고 `view_attachment`를 등록한다. iiSDKCore가 제공하는 `main()`과 이벤트 루프가
앱의 실행·종료를 담당한다. 예제의 [CMake 연결](../example/GettingStarted/CMakeLists.txt)을
복사해 새 이름과 파일을 등록할 수 있다.

명령행에서 파서와 컴파일러를 각각 사용할 수도 있다. `lvui-check`는 구문 확인이고,
최종 네이티브 C++의 타입·링크 성공까지 보장하지 않는다.

```sh
build/lvui-check example/GettingStarted/App/Hello.lvui
build/lvui-compile --name Hello example/GettingStarted/App/Hello.lvui
```

두 번째 명령은 생성 코드를 표준 출력으로 내보낸다. 백엔드가 필요한 파일은
`--include ReadingListBackend.h`처럼 헤더를 지정하고 C++ 컴파일러가 그 헤더를 찾도록
설정한다. 교차 컴파일 시에는 대상용 바이너리가 아닌 호스트용 `lvui-compile`을
`LVUI_HOST_COMPILER`로 지정한다. 자세한 선택지는 [AOT 계약](NativeAOT.md)과
[스타일 토큰](StyleTokens.md)을 따른다.

## 반응형 상태, 입력과 재사용 View

`import vm ReadingListBackend`는 뷰가 활성화될 때 해당 C++ 백엔드를 소유한다.
일반 public C++ 멤버를 사용하며 별도의 LVUI 기반 클래스나 등록 매크로는 필요하지
않다. 소유한 백엔드는 같은 활성화의 여러 클릭에 재사용되고 자식 뷰는 필요한 값을
전달받는다. 실제 데이터 변경이 UI에 통지되어야 하는 필드는
`ii::core::Observable<T>`로 두고 `.set()`으로 발행한다.

```lvui
import vm ReadingListBackend
let ReadingRow(title: String): View = HStack {
    Label(text: title)
}
Window {
    VStack {
        InputField(text: vm.draft, onEdited: vm.edited, onSubmit: vm.add)
        PushButton(text: "Add", onClick: vm.add)
        for (item in vm.items, key: item.id) {
            ReadingRow(title: item.title)
        }
    }
}
```

`vm.add`는 나중에 호출할 메서드 참조이며 `vm.add()`는 즉시 호출하는 표현이다.
`onEdited`가 받은 문자열을 백엔드에 저장하면 입력과 모델이 동기화된다. `let`으로
선언한 파생 값은 지원되는 의존성을 추적하고, `var`는 변경 가능한 값이며,
`@Static let`은 활성화 시점의 값을 고정한다. 반응형 값이 쓰기 권한을 부여하는 것은
아니다. 키 기반 반복에는 위치가 바뀌어도 고유하고 안정적인 ID를 사용한다.
행의 지역 상태는 행이 퇴역하면 사라지므로 오래 유지할 상태는 상위 모델이 소유한다.
[상태 바인딩](NativeBindings.md), [재사용 구성](Components.md),
[반복과 가상화](Repetition.md)를 참조한다.

창의 강조색은 `Window(accent: .purple)`처럼 지정하거나 백엔드의
`ii::core::Observable<lvui::WindowAccent>` 또는 이름 문자열에 연결할 수 있다.
기본 버튼에는 `width`, `primaryColor`, `hoverColor`, `pressedColor`, `borderColor`
등을 직접 지정할 수 있으며 명시한 버튼 색상은 창 기본색보다 우선한다. 모든 위젯의
테마 상속이 완성된 것은 아니므로 [Window](NativeWindow.md)와
[버튼](NativeButton.md)의 지원 범위를 확인한다.

개별 계층 행의 `countView`에는 `Label(text: vm.badge)` 같은 단일 View를 전달할 수
있다. `count`가 음수이면 View를 숨기고 0 이상이면 다시 표시한다. 계층 목록이
자동으로 생성하는 행의 사용자 대리자는 아직 별도 구현이 필요하다.

백그라운드 작업의 수명은 일반 콜백이나 `Observable`만으로 자동 보장되지 않는다.
비동기 작업이 필요하면 [활성화 소유 작업](ActivationWork.md)을 검토하고, 뷰보다
오래 살아야 하는 작업은 앱이 명시적으로 소유해야 한다.

## 표 데이터와 교체 가능한 경계

Inventory는 간단한 `TableMemory`에 열·행을 저장하고 `TableController`를 표에
전달한다. 편집에는 같은 데이터의 `TableEditor`가 필요하며 정렬에는
`TableOrderEditor`가 필요하다. 셀 확정, 정렬, 범위 붙여넣기 뒤에 백엔드가
`modelRevision`을 갱신해 화면에 변경을 알린다. 선택이나 스크롤 자체를 문서 편집으로
취급하지 않는다. 예제에서 Enter는 편집 시작과 확정, Escape는 취소다.

표는 양축의 보이는 셀을 그리며 편집·범위 클립보드·정렬과 크기 조절을 지원한다.
셀 병합, 행·열 삽입 삭제 UI, 셀별 접근성과 실제 플랫폼 렌더링은 아직 남아 있다.
제품 데이터에 연결할 때는 [표 모델](OwnedTableModel.md), [표 호스트](NativeOwnedTable.md),
[셀 편집](TableEditing.md)과 [정렬](TableSorting.md)의 리비전 계약을 따른다.

## 현재의 제약과 토큰 차이

- `InputField`는 자체 편집·선택·IME 상태를 제공하지만 Android와 Web에서는 필요한
  텍스트 배치 서비스가 없어 생성할 수 없다. 자동 줄바꿈, 실행 취소·재실행,
  더블 클릭 단어 선택과 실제 운영체제별 입력기·접근성 검증은 남아 있다.
- 스타일 카탈로그의 208개 값은 저장된 Figma 스냅샷이다. 조사 시 LVUI에는 Figma에서
  삭제된 `Action/primary`, `Action/success`, `Action/warning`, `Action/danger` 별칭이
  남아 있고 기본 색상 생성에도 이전 별칭이 사용된다. 최신 Figma 악센트 역할로의
  전환이 끝났다고 가정하지 않는다. 이 예제 작업은 토큰을 수정하지 않는다.
- 헤드리스 CTest, 앱 바이너리 빌드, 실제 OS 창·IME·GPU·접근성 확인, 설치본 검증은
  서로 다른 증거다. 각각 수행한 결과만 해당 범위에 적용한다.

## 이 예제의 검증 기록

2026-10-29 macOS arm64의 LVUI 현재 작업본에서 세 `.lvui` 파일의 구문 검사와
생성 C++ 및 세 앱 타깃 빌드를 통과했다. 호스트는 같은 날짜의 iiGUI 검증용 패키지,
Core와 색상 값 패키지는 빌드에 지정된 별도 패키지를 사용했다. 헤드리스 통합 검사는
초기 화면, 입력·추가·비우기·재추가, 표 셀 확정·정렬·붙여넣기와 활성화 해제를
확인했다. 앱 창을 열거나 GPU·운영체제 입력·접근성을 실행 검증하지 않았고,
`~/.local/SDK` 설치본을 갱신하거나 설치 소비자를 검증하지 않았다.
