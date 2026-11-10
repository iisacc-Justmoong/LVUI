# LVUI 독립 버튼

## 아이콘 버튼 공통 치수 명세

Congregation의 메인 도구 레일과 명령 막대는 원본 LVUI
[IconButton borderless 변형](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI?node-id=44-595)을
사용한다. 원본 25개 톤·상호작용 변형은 모두 22×22 논리 px 버튼, 18×18 아이콘,
각 2px 안쪽 여백과 8px 반경을 사용한다. 이 고유 치수는 Figma 컴포넌트 스냅샷에서
LVUI가 생성·소유하며 제품이 별도로 지정하지 않는다. C++ 공통 기본값과 선언형
`IconButton`은 같은 치수를 사용하고, 다른 버튼 유형은 기존 기본값을 유지한다.
이 치수는 변수로 연결되지 않은 Figma 컴포넌트 자체의 고정 기하이며,
`catalog/lvui-icon-button-geometry.json`에서 일관성을 검사하여 공개 헤더로 생성한다.
동일한 숫자의 간격 토큰을 아이콘 치수로 혼용하지 않는다.
제품의 두 버튼, 공유 2px 간격과 4px 외부 여백으로 54px 도구 레일을 계산한다.
이는 디자인의 고정 두 열을 모델링한 배치이며 제품 전용 크기 토큰이 아니다.

기본 `AbstractButton`, `PushButton`, `LabelButton`과 `IconButton`의 네이티브
호스트는 `iiGUI/NativeButtonView.h`가 소유한다. iiGUI가 상태·기하·입력·접근성·
모션·배경·텍스트·아이콘·포커스 그리기와 수명을 처리한다. `LVUI/NativeButton.h`는
기존 선언에서 선택한 디자인 토큰, 아이콘 공급자와 콜백 계약을 iiGUI 정책에
연결한다. `LVUI/Button.h`는 선언 값과 토큰만 제공하며 중복 계산 모델은 제거했다.
직접 C++ 소비자는 `ii::gui::ButtonModel`을 사용하거나 `ButtonBinding`에 iiGUI
모델·호스트·디코더·위젯 타입을 명시적으로 주입한다. LVUI 디자인 값과 바인딩은
iiSDKCore 외 SDK를 직접 참조하지 않는다.

`AbstractButton`은 Society 계정 버튼처럼 한 개의 내용 View를 `{}`에 합성할 수 있다.
여러 항목은 스택으로 감싼다. 내용 View가 있을 때 `text`, `icon`, `iconName` 입력은
함께 사용하지 않는다. 내용은 LVUI가 소유한 컴포넌트이며 버튼의 배경을 제품별로
재정의하지 않는다. 생성된 버튼을 제거하면 내용과 입력 연결도 함께 퇴역한다.

```lvui
import vm EditorBackend
Window {
    VStack {
        PushButton(text: vm.title, enabled: vm.ready, onClick: vm.save)
        IconButton(icon: vm.toolIcon, accessibilityName: "Select tool", onClick: vm.selectTool)
        IconButton(iconName: "generaladd", accessibilityName: "Add", onClick: vm.add)
    }
}
```

`text`, `enabled`, `motionEnabled`, `enterKeyActivation`, `tone`, `height`, `horizontalPadding`, `cornerRadius`,
`accessibilityName`, `iconMode`, `iconSize`, `icon`, `iconName`, `iconSource`,
`iconRevision`은 해당 타입의 상수 또는 반응형 입력을
받는다. 동적 tone은 `lvui::ButtonTone` 또는 호스트의 대응 enum을 받을 수 있다.
`icon`은 앱이 제공하는 `shared_ptr<const ii::gui::RasterImage>`이고 `iconName`은
[LVUI의 내장 아이콘 또는 교체 가능한 공급자](NativeIcons.md)를 조회한다. 두 입력을
동시에 사용할 수 없다. 이름을 찾지 못하거나 SVG가 유효하지 않으면 오류를 반환하고
기존 버튼을 유지한다. 아이콘 모드에서는 유효한 원본이 필요하다. 파일·URL 로딩은
이 경로에서 제공하지 않는다.

추가로 `width`, `iconSpacing`, `fontSize`, `lineHeight`, `textOffsetY`,
`borderColor`, `borderWidth`를 지정할 수 있다. `fontSize`만 지정하면 줄 높이도
그 크기를 사용한다. `borderColor`만 지정하면 기본 선 두께를 사용하며,
`borderWidth`만 지정하는 선언은 오류다.

`primaryColor`, `hoverColor`, `pressedColor`는 primary 톤에 적용한다.
`normalColor`, `normalHoverColor`, `normalPressedColor`는 default 톤에 적용하고,
`dangerColor`, `dangerHoverColor`, `dangerPressedColor`, `disabledBackgroundColor`, `textColor`, `disabledTextColor`는 각각의
상태 색상을 정한다. borderless 톤의 텍스트는 `primaryColor`를 사용한다.
기본·primary·destructive 라벨의 기본 전경은 LVUI 제목 텍스트 토큰의 흰색
90% 불투명도이며, 비활성 라벨에는 비활성 텍스트 토큰을 사용한다.
`selectedColor`는 선택된 버튼의 배경을 지정하며 반응형 값으로 바꿀 수 있다.
명시된 색상은 [창 강조색](NativeWindow.md)보다 우선하며, 반응형 입력을 허용한다.
잘못된 후보는 이전 상태를 유지한다.

현재 Figma LVUI의 레이블형과 아이콘형 버튼 각각 25개 상태를 대조했다. 기본
톤은 평상시 `Panel/surfaceSolid`, 호버 시 `Panel/surfaceAlt`, 누름 시 현재
강조색의 `Selected`를 사용한다. 파괴적 톤은 `Accent/Red`의 Default·Hover·
Pressed 토큰을 각각 사용한다. 기본 빨강이 아닌 `dangerColor`만 전달한 기존
호출은 종전처럼 호버와 누름 색상을 그 값에서 계산한다. 두 상태 색상을 명시하면
그 값이 우선한다. 이는 기본 상태의 재질 연결을 대조한 결과이며 모든 상태의
실제 GPU 표시나 버튼 내용의 변형 모션까지 검증한 결과는 아니다.
선언형 기본 버튼은 컴파일 시 선택한 `Color.red.hover`와 `Color.red.pressed`를
각 상태에 전달한다. `dangerColor`를 명시하지 않고 상태색만 명시하면 해당
상태색이 공통 토큰보다 우선하며, `dangerColor`를 명시한 경우에는 나머지
상태색을 기존 버튼 모델의 규칙으로 계산한다. 공통 상태색의 기준 기본색을
별도로 보관하므로 C++에서 `options()`의 기본 빨강만 변경해도 명시적 상태색이
없는 한 새 색상에서 호버·누름을 계산한다. `ButtonPalette`에 기준색 필드가
추가되었으므로 C++ 소비자는 새 헤더로 다시 빌드해야 한다. 실행 중 전역 테마
구독은 별도다.
이 변경 후 macOS arm64 구성의 LVUI 전체 빌드와 헤드리스 회귀 640건, 별도
설치 패키지 경계 검사가 통과했다. 별도 빌드 디렉터리를 만드는
`LVUI.NativeInstalled`는 제외했으며 실제 앱 창·GPU 표시는 검사하지 않았다.
대조한 [LVUI 라이브러리](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI)의
레이블형 컴포넌트 세트 키는 `254869f4162699c28ce35bc6eca99f0ab349fd7d`,
아이콘형은 `eece7627fc7a9bf1c68d3970a93767672aedb757`이다.

primary·default·borderless·destructive·disabled 색상과 기본 치수는 LVUI의
스타일 토큰 스냅샷으로 생성한다. 눌림은 호버보다 우선하며 disabled tone은
사용자 요청 enabled 값과 별도로 입력을 막는다. 반응형 값은 하나의 후보로
검증한 뒤 기존 동작 표면에 반영한다. 잘못된 크기·텍스트·색상·enum 또는 누락된
아이콘은 이전 버튼을 유지하고 정상 후보가 들어오면 복구한다. 텍스트는 현재
해결된 애플리케이션 글꼴을 사용하며 글꼴이 바뀌어도 버튼을 재생성하지 않는다.
할당된 너비보다 긴 문구는 사용 가능한 영역에 맞추어 줄임 표시한다. 원문과
접근성 이름은 줄이지 않는다.

기본 및 borderless 톤의 누름 배경은 현재 창 강조색의 `Selected` 토큰을
사용한다. 창의 강조색이 바뀌면 누름 색상도 갱신한다. `normalPressedColor`를
명시하면 이 값이 창 색상보다 우선한다. 이 계약은 모델과 선언형 생성 경로에
모두 적용한다. 아이콘과 라벨 버튼의 포커스 표시는 버튼 사각형에서 3 논리 픽셀
바깥쪽의 둥근 테두리로 그리며 입력 영역이나 레이아웃을 확대하지 않는다. 상위
스크롤 영역과 클립은 계속 적용하고, 비활성 버튼에는 표시하지 않는다. 표시는
호스트의 포커스 상태를 따르며
키보드와 포인터로 얻은 포커스를 구별하지 않는다.

버튼은 비소유 스타일 포트를 통해 부모가 임시 무테 톤을 적용하도록 허용한다.
이때 원래 반응형 톤을 보존하고 임시 설정을 해제하면 최신 원래 톤을 복원한다.
disabled 톤은 임시 스타일로 활성화되지 않는다. 폐기된 버튼은 포트를 통해
변경할 수 없다. [세그먼트 묶음](NativeSegmentedControl.md)은 이 포트를 조합한다.
버튼 모델은 필요한 경우 명시적 너비와 텍스트 세로 보정값도 보유한다.
[도움말 버튼](NativeHelpButton.md)은 이 설정을 원형 기본값에 사용한다.
앱 측 C++ 조합은 선택적 테두리 색상과 폭을 모델에 지정할 수 있다. 테두리는 버튼
내부에 그리며 포커스 표시와 독립적이다. 기본값은 테두리 없음이다.
`NativeButton::options()`는 임시 부모 스타일을 적용하기 전의 원래 입력을 복사해
돌려준다. 앱은 이를 수정해 `set_options()`에 다시 전달할 수 있으며, 소유 스레드가
아니거나 위젯이 폐기되었다면 조회도 오류를 반환한다.

이 경로는 앱이 `WidgetPaintState::font_family`를 제공하는 iiGUI 호스트를
연결해야 한다. LVUI 선언 값은 iiGUI와 LVRS를 직접 포함하지 않는다. 현재 범위는
위 입력과 아래 입력 이벤트이며 원본의 모든 모션·
상속 API와 실제 플랫폼 접근성 검증은 아직 완료되지 않았다. 호스트 검사는
창을 열지 않으며 GPU·기기 실행 증거로 취급하지 않는다.

## 누름 이벤트 계약

`AbstractButton`, `PushButton`, `LabelButton`, `IconButton`, `HelpButton`은
`onPressed`, `onReleased`, `onCanceled`와 `onClick`을 각각 받을 수 있다.
포인터나 키보드로 활성화하면 누름, 놓기, 클릭 순서로 호출한다. 포인터를
영역 밖에서 놓으면 놓기까지만 전달하며, 누름 중 포커스를 잃거나 Escape를
누르면 취소만 전달한다. 직접 `activate()`를 호출할 때는 클릭만 발생하고,
비활성 버튼은 이벤트를 받지 않는다. 이벤트는 위젯 소유 스레드에서 처리한다.

C++ 조합에서는 `NativeButtonEvents`를 `NativeButton::set_events()`로 교체하고
클릭은 생성 시 전달한다. 이벤트 처리 중 다른 이벤트를 교체하거나 버튼을
폐기할 수 있으며, 폐기된 버튼이나 다른 스레드에서의 변경은 오류를 반환한다.
원시 위젯의 `set_control_events()`를 직접 사용하면 내부 전달자를 교체하므로
공통 버튼에는 `set_events()`를 사용한다. `AlertButton`도 같은 입력 경로를
사용하며 그 자체의 메서드·클릭 순서를 유지한다.

`enterKeyActivation`은 상수 또는 반응형 불리언 입력이며 기본값은 `PushButton`,
`LabelButton`, `IconButton`에서 켜지고 `AbstractButton`, `HelpButton`, `AlertButton`에서
꺼진다. Space·포인터·접근성·명시적 활성화에는 영향을 주지 않는다. 허용된 Enter는
누를 때 피드백을 표시하고 같은 키를 놓을 때 한 번 활성화한다. 누름 중 설정을 끄거나
포커스·활성을 잃으면 취소하며 해제 시 클릭하지 않는다. C++에서는 버튼 설정의
`enter_key_activation`으로 제어한다.

## 버튼 메서드 계약

`AbstractButton`, `PushButton`, `LabelButton`, `IconButton`, `HelpButton`,
`AlertButton`, `DropdownButton`, `IconMenuButton`, `LabelMenuButton`, `ComboBox`,
`Stepper`는 `method` 한 개와 `methods` 목록을 받을 수 있다. `method`를 먼저,
중첩된 목록은 작성 순서대로 평탄화하여 호출한 뒤 `onClick`을 호출한다.
스테퍼는 이후 기존 `onStepped`를 전달한다. 입력 콜백은 인자가 없거나
`lvui::native::ButtonEventValue<Widget>`를 받을 수 있으며, 이벤트에는 앱이 주입한
원본 위젯, `clicked` 트리거, 작성된 활성 상태와 실제 활성 상태, 작성된 톤이 담긴다.
`AlertButtonEvent`는 현재 앱 조합에서 같은 이벤트를 가리킨다.
스테퍼의 설정이 메서드나 클릭 처리 중 바뀌면 이미 선택된 방향의 `onStepped`는
오래된 값이므로 전달하지 않는다. 메서드 목록과 클릭 자체는 활성 상태가 유지되면
순서대로 처리한다.

상수 목록과 반응형 목록을 모두 지원한다. C++ 목록에는 함수, 인자 없는 함수,
`invoke` 또는 `trigger`를 제공하는 값, 중첩 범위와 선택적 값을 넣을 수 있다.
공유 소유 객체는 복사본이 아닌 원래 인스턴스를 호출하며 목록이 보유한다.
비어 있는 항목은 건너뛰며 목록의 호출 가능한 항목은 최대 256개이다. C++에서는
`set_method()`와 `set_methods()`로 교체하고, 중첩 값은 이벤트 타입을 지정하는
`collect_button_methods<Event>()`로 평탄화할 수 있다. 이벤트의 트리거 문자열은 자체 소유한다. 호출 목록과 이벤트는 클릭 시작
시점의 스냅샷이며, 처리 중 변경된 목록은 다음 클릭부터 사용한다. 처리 중 위젯이
폐기되거나 비활성·숨김·차단되면 나머지 메서드와 클릭은 전달하지 않는다.
다른 스레드나 폐기된 위젯의 변경은 오류이며, 잘못된 목록은 이전 목록을 유지한다.
명시적 활성화는 구조상 표시되는 버튼이라면 배치 전이나 스크롤·클립 영역 밖에서도
허용한다. 실제로 숨겨졌거나 팝업에 차단된 버튼은 계속 처리하지 않는다.

C++ `invoke_methods()`는 클릭을 발생시키지 않고 같은 목록을 명시적으로 실행한다.
기본 트리거는 `clicked`이며 임의 문자열로 바꿀 수 있다. 명시 호출은 비활성·숨김
상태에서도 허용하고 이벤트에 그 상태를 기록한다. 호출 중 폐기되면 나머지를
중단한다. 현재 C++ 콜백의 반환값은 소비하지 않으므로 LVRS의 수동 호출 결과 목록은
제공하지 않으며, 선언형 언어에서 임의 위젯의 명시 호출도 아직 제공하지 않는다.

## 상태색 전환

네이티브 버튼의 배경 RGBA는 LVRS QML `StateColorBehavior`와 같은 기본
130ms OutCubic으로 전환한다. 호버·누름·해제와 `set_options()`를 통한 선택·
팔레트 갱신의 목표가 바뀌면 현재 표시색에서 다시 시작한다. 입력 처리와 클릭
콜백은 전환 완료를 기다리지 않는다. 비활성화, 공유 모션의 축소 설정 또는
프레임을 제공하지 않는 호스트에서는 목표색에 즉시 도달한다. 공유 모션의 속도와
진행 중 정책 변경을 반영하고 버튼 폐기 후 프레임 콜백은 위젯에 접근하지 않는다.
호스트가 반환된 원시 위젯에서 `set_checked()`를 직접 호출하면 해당 선택색은 즉시
표시되며, 이후 입력 전환은 그 표시색에서 시작한다. 선언형 반응형 값과
`set_options()`를 이용한 선택 변경은 일반 전환을 따른다.

포커스 테두리는 배경색과 별도로 불투명도를 160ms OutCubic, 크기를 360ms
OutBack으로 전환한다. 비활성 상태의 크기는 원래 테두리의 96%이며 중앙을 기준으로
확대된다. 포커스를 잃는 중에도 표시가 서서히 사라지고 전환 도중 다시 포커스를
받으면 현재 표시 상태에서 되돌아간다. 전역 축소 모션과 개별 모션 해제, 프레임을
제공하지 않는 호스트에서는 목표 상태로 즉시 정착한다. 이 표시의 전환은 클릭 영역과
부모의 클리핑을 변경하지 않는다. 버튼 내용의 텍스트·아이콘 변형 모션과 LVRS의
`motionStrength`와 `releaseOnSignal` 입력은 아직 제공하지 않는다.
포커스 표시의 시간·곡선은 LVRS QML에서 이식한 것이며 실제 GPU 픽셀
동등성을 검증한 결과는 아니다.

2026-11-03에 개명된 LVUI의 `IconButton` borderless 변형을 다시 확인했다.
기본·호버·누름·해제 모두 버튼은 22×22, 아이콘은 18×18이며 아이콘 위치는
동일하다. 프로토타입에는 호버 진입·이탈 100ms, 누름 60ms, 누름 해제
80ms, 해제 상태에서 복귀 120ms의 `EASE_OUT` 전환이 정의되어 있다.
이는 확인한 Figma 변형의 상태와 프로토타입 시간이며, 이전 QML의
`InteractionMotion`이 제공한 변형 효과와는 별개의 사실이다.

### Figma 버튼 상태 전환

개명된 LVUI의 레이블형 `44:599`와 아이콘형 `1243:1226` 컴포넌트 세트는
활성 네 톤 모두 같은 프로토타입 전환을 사용한다. 호버 진입·이탈은 100ms,
누름은 60ms, 누른 상태에서 놓거나 벗어나는 전환은 80ms이다. 정상 해제 뒤
120ms의 해제 상태에서 벗어나는 전환은 120ms이며, 해제 상태의 기본 표면은
호버 표면과 같다. 누름 도중 벗어난 경우에는 해제 상태를 만들지 않는다.
선택·팔레트의 프로그램 변경은 기존 130ms 정책을 유지한다. 공유 속도·축소
모션, 개별 `motionEnabled`, 비활성화와 프레임 없는 호스트의 즉시 정착을
보존한다. 진행 도중 역전되면 마지막 표시색에서 다시 시작한다.
시간은 `catalog/lvui-button-motion.json`에서 생성한 공통 기본값을 사용한다.
포인터 누름 중 이탈했다가 들어오면 눌림이 아닌 호버 표면을 표시하되 실제
클릭의 성립 여부는 iiGUI 입력 계약에 맡긴다. 포인터 위치 변경은 키보드 누름을
취소하지 않는다. 이를 위해 `Widget::press_origin()`을 제공하는 iiGUI의 현재
헤더와 라이브러리가 함께 필요하다.

원본의 `EASE_OUT`은 Figma 문서상 CSS ease-out과 유사한 곡선이다. 정확한
제어점은 원본 데이터에 없으므로 표준 CSS ease-out을 근사로 사용하고 GPU
픽셀이나 원본 곡선과의 완전 일치를 주장하지 않는다. 헤드리스 모형과 주입된
프레임 검사는 진입·누름·해제·이탈·역전·포인터 재진입·키보드 입력 및 축소
모션을 확인한다.

### 개별 모션 설정

`AbstractButton`, `PushButton`, `LabelButton`, `IconButton`, `HelpButton`과
`AlertButton`은 `motionEnabled`의 상수·반응형 입력을 지원하며 기본값은 `true`이다.
C++ 설정에도 대응하는 `motion_enabled`를 제공한다. 해당 버튼에서 진행 중인 색상과
포커스 표시 전환을 끄면 현재 목표 상태로 즉시 정착하며, 다시 켜면 다음 목표
변경부터 전환한다.
다른 버튼의 설정과 전역 모션 정책은 변경하지 않는다. 전역 비활성화나 축소 모션은
개별 버튼이 활성화되어 있어도 우선한다. 비활성 설정을 유지하는 동안 상태색은
즉시 바뀌며 클릭과 접근성 상태는 그대로 동작한다.

`checkable` 버튼은 활성화할 때 checked 상태를 토글한다. checked일 때는 톤을
교체하지 않고 앱 악센트의 Selected 배경을 사용하며, 누르는 동안과 비활성 상태는
기존 버튼 상태 우선순위를 따른다. 코드 생성 경로는 `checkable`, `checked`를 받는다.
직접 C++에서 조합할 때 `ButtonPalette.selected`를 지정하지 않으면 현재 `primary`
색상으로 선택 배경을 계산하며, 지정한 경우에는 해당 색상을 사용한다.
앱이 단일 선택을 소유하는 경우 `checkable`을 끄고 `checked`를 반응형으로 제공하면
선택된 버튼을 다시 클릭해도 자체적으로 선택을 해제하지 않는다.

세그먼트의 `selectionStyle={.segmented}`는 선택 상태를 공통 Button의
`default` 종류로 표시하고, 선택되지 않은 항목은 원래 종류를 유지한다.
따라서 일반적인 `LabelSegmentedControl`의 borderless 자식은 선택 시 기본
표면과 제목 글자를 사용하며 hover·pressed와 비활성 상태도 해당 공통 버튼의
규칙을 따른다. 기본 `selectionStyle`은 기존 악센트 선택 방식이다. 이 정책은
표시만 결정하며 선택의 소유권이나 클릭 동작을 바꾸지 않는다.

`LVUI.WindowsAccessibility.GeneratedButton`은 선언에서 생성한 버튼과 백엔드를
iiGUI 위젯 트리에 마운트하고 모의 Windows 접근성 어댑터에 연결한다. 버튼을
퇴역·재마운트한 뒤 이전 접근성 객체와 대기 중인 동작이 새 버튼을 호출하지 않고,
새 객체의 동작은 현재 백엔드에 전달되는지 확인한다. 검사는 실제 Windows 창이나
스크린 리더를 실행하지 않는다.

2026-11-07 macOS arm64에서 iiGUI 버튼 모델·호스트와 독립 설치 소비자,
LVUI 버튼 및 시트·모달·목록·색상 피커 등 버튼 조합 24건의 헤드리스 검사가
통과했다. 실제 창·GPU와 다른 운영체제의 실행은 검증하지 않았다.

버튼 메서드 이벤트·목록·등록 계약은 위젯 타입을 주입받으며 iiSDKCore 외 SDK를
직접 참조하지 않는다. macOS arm64에서 Core 단독 계약과 버튼 계열 생성 코드·호스트,
재진입·수명과 설치 헤더를 창 없이 검사했다.

버튼 바인딩 자체는 Core 단독으로 컴파일된다. macOS arm64에서 버튼·모달·색상
선택기와 버튼 계열의 생성 및 수명 검사, 설치 헤더와 Python 컴파일러 연결을
창 없이 확인했다. 다른 운영체제의 실제 렌더링은 검사하지 않았다.
