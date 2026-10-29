# LVRS 자체 위젯 코드 생성

LVRS의 네이티브 구현은 `lvui_generate(... FRAMEWORK LVRS ...)` 또는
`lvui-compile --framework LVRS`로 선택한다. 생성한 코드는 `LVRS/Widgets.h`와
`lvrs::Widgets`를 사용하며 최종 앱은 `LVRS::LVRS`에 연결한다.
LVUI 컴파일러 자체는 LVRS·iiGUI 런타임에 의존하지 않는다.

이전 문서의 Cocoa 컨트롤 래핑 방식은 현재 구현을 설명하지 않는다.
현재 iiGUI C++ 위젯이 레이아웃·입력 상태·그리기를 소유한다. Cocoa는 단일 표면과
이벤트·IME·접근성 연결을 제공하며, 버튼·레이블마다 OS 위젯을 만들지 않는다.

현재 AOT는 `ApplicationWindow`, `Window`, `VStack`, `HStack`, `ZStack`, `Spacer`,
`Label`, `AbstractButton`, `PushButton`, `LabelButton`, `CheckBox`, `RadioButton`,
`ToggleSwitch`, `InputField`, `Slider`, `ProgressBar`, `IconButton`, `HelpButton`,
`DropdownButton`, `IconMenuButton`, `LabelMenuButton`, `Tab`, `TabBar`, `ComboBox`,
`Stepper`, `LabelSegmentedControl`, `IconSegmentedControl`, `Menu`, `ContextMenu`,
`MenuItem`, `ContextMenuItem`, `MenuDivider`, `ContextMenuDivider`, `ColorPicker`, `ColorPickerSurface`,
`ColorField`, `Popover`, `ColorPickerButton`, `Modal`, `Alert`, `AlertButton`, `Sheet`,
`TableCellItem`, `TableRow`, `TableHeader`를 C++ 위젯 호출로 생성한다.
각 컴포넌트의 지원 입력은 LVRS의 `catalog/native/parity.json`을 기준으로 한다.
[ProgressBar](NativeProgress.md)는 단순 iiGUI 막대 대신 원본의 상태 제공자·스타일·모션을
가진 네이티브 컨트롤을 생성한다. 직접 scoped import와 타입 지정 View 입력도 연결한다.
현재 지원은 기본 동작의 부분 이식이며 기존 LVRS 전체의 동등성 달성을 의미하지 않는다.

[페이지와 모델 수명](NativeRouteModels.md)은 스코프의 네이티브 PageRouter와
ViewModelRegistry를 연결한다. LVUI는 반응형 표시와 C++ 명령 참조를 생성하며 경로·권한·
정리를 C++ 컨트롤러가 소유한다. 생성 팩터리와 설치 소비자 빌드, 최종 실행 증거를 구분한다.

`Sheet`는 `presentation: .automatic/.mobile/.desktop`, `detent: .fit/.medium/.large`,
제목·설명·표시 플래그·크기/여백·안전 영역·닫기 정책·RGBA 색상·motion과 밀리초 단위의
정수 animationDuration을 받는다. 본문은 네이티브 생성 시 한 번 실행하는
`SheetContentFactory`이며 닫힌 뒤에도 같은 C++ 객체를 유지한다. 여러 본문 View의 ID도
마운트 전체에서 유일해야 한다. 자식들은 부모 스코프 backend를 빌린다. Modal의 열림
단위 본문과 수명을 혼동하지 않는다. [Sheet 계약](../../LVRS/docs/native/Sheet.md)에 현재
범위와 전환 신호·일반 반응형·플랫폼의 남은 차이를 명시한다.

`onClick`·`onSubmit`은 인자 없는 C++ 메서드 참조이고, `onToggled`는 `bool`,
`onEdited`는 `std::string`, `onMoved`는 `double`, `onCurrentIndexChanged`는 `int`,
`onColorEdited`는 `ii::gui::WidgetColor` 값을 전달한다.
호출 문법을 메서드 참조 대신 쓰거나 알려지지 않은 입력을 주면 컴파일 진단을 반환한다.
가져온 C++ 객체는 View의 스코프에 한 번 생성하며 Core가 작업을 정리한 뒤 해제한다.

LVRS 프레임워크를 선택한 창은 `accent: .green`과 같은 악센트 선택을 지원한다.
blue·purple·rose·red·orange·yellow·green·gray·brown·cyan·teal·white를 제공하며
상태 색상과 2% 창 혼합이 같은 테마에서 계산된다.

이 레거시 경로의 아이콘 버튼은 `iconName`, `iconSize`, `iconMode`를 지원한다.
기본 경로의 아이콘 버튼과 드롭다운은 [LVUI 독립 아이콘](NativeIcons.md)을 사용한다.
이 문서의 탭과 탭 묶음은 `FRAMEWORK LVRS` 경로를 설명한다. 기본 경로는 독립
단일 탭과 직접 자식 탭 묶음을 제공하며 지원 범위는 [현재 현황](NativeTabs.md)을 따른다.
`Tab`의 `iconName`은 필수이며 누락·공백 값은 소스 위치가 있는 AOT 오류이다.
유효하지 않은 자산 이름이나 미지원 SVG 기능은 마운트 시 오류를 반환한다.

```lvui
TabBar(id: "tabs", currentIndex: 0, onCurrentIndexChanged: vm.selected) {
    Tab(text: "Overview", iconName: "nodesfolder")
    Tab(text: "Details", iconName: "generaladd", badge: "3", tabStyle: .surface)
}
```

TabBar는 정적 자식 Tab의 동일 너비 배치, 현재 인덱스, 자동 단일 선택과 비활성 탭
건너뛰기를 구현한다. 좌우·Home·End 키는 포커스만 이동하고 클릭·Enter가 선택한다.
스크롤 너비 정책과 동적 모델 바인딩은 아직 지원하지 않는다. 드롭다운 버튼의 콜백은
앱 메서드에 연결되며 메뉴를 자동 생성하지 않는다. 원본 LVRS도 메뉴 열기를 콜백에 연결한다.

`ComboBox`는 `text`, `width`, `enabled`, `tone: .primary/.borderless`,
`arrow: .upDown/.up/.down`과 `onClick`·`onPressed`·`onReleased`·`onCanceled`를 받는다.
메뉴와 목록을 소유하지 않는 클릭 필드이다. `Stepper`도 같은 화살표와 이벤트를 받으며
추가로 `cornerRadius`, `onStepped: vm.step`을 지원한다. 방향 콜백은 `int`의 `+1/-1`이고,
기본 18×18 크기의 위·아래 절반, 방향키 또는 가상 접근성 행동이 결정한다.
숫자 갱신·반복 타이머를 암묵적으로 추가하지 않는다.

```lvui
HStack {
    ComboBox(text: "Choice", onClick: vm.openMenu)
    Stepper(arrow: .upDown, onStepped: vm.step)
    LabelSegmentedControl(shapeStyle: .cylinder) {
        LabelButton(text: "One", onClick: vm.one)
        LabelButton(text: "Two", onClick: vm.two)
    }
    IconSegmentedControl {
        IconButton(iconName: "nodesfolder", onClick: vm.folder)
    }
}
```

세그먼트 행은 `horizontalPadding`, `verticalPadding`, `spacing`, `borderWidth`,
`cornerRadius`, `shapeStyle: .roundRect/.cylinder`, `forceBorderlessTone`을 받는다.
label 행 기본 수직 여백은 3.5, icon 행은 4이며 자식 버튼의 콜백은 독립적이다.
`currentIndex`와 같은 선택 모델 입력은 오류이다. C++의 불투명 배경·테두리 색상은
현재 AOT 입력에 노출하지 않으며, 동적 자식·반응형 스타일·모션·method registry는 남아 있다.

일반 반응형 필드 바인딩, SVG 필터·전체 재질·모션, 다른 예약어의 코드 생성과 각 OS
입력 어댑터는 계속 구현해야 한다. 이전 AOT 검증 기록과 현재 자체 렌더링 검증을 구분한다.

## 메뉴와 컨텍스트 메뉴

메뉴는 일반 배치 공간을 차지하지 않는 자체 렌더링 팝업이다. 메뉴·항목·구분선의 일반형과
압축형 여섯 타입을 지원하며, C++에서 `open_at`, `open_for`, `close_popup`으로 제어한다.
일반형은 24 px 행·상하 4 px, 압축형은 18 px 행·상하 8 px 여백이며, 압축 레이블은
내장 Inter Regular 12 px를 사용한다. 일반 레이블과 단축키는 Pretendard이다.

```lvui
VStack(id: "content") {
    InputField(id: "field", placeholder: "Right click")
    ContextMenu(targetId: "field", onItemTriggered: vm.selected) {
        ContextMenuItem(label: "Open", shortcut: "⌘O", hasChildItems: false, onClick: vm.open)
        ContextMenuDivider()
        ContextMenuItem(label: "Unavailable", state: .inactive, hasChildItems: false)
    }
    Menu(id: "quick", items: ["First", "Second"], showIconSlot: false,
         onItemTriggered: vm.selected)
}
```

`targetId`는 같은 View 안의 위젯 ID이며 앞·뒤 선언을 모두 참조할 수 있다. 컴파일 단계에서
확인한 C++ 위젯 핸들을 `set_context_menu`로 연결한다. 오른쪽 마우스 요청과 접근성의
Show Menu 동작으로 해당 팝업을 연다. 알려지지 않은 대상 ID는 AOT 오류이다.

`items`는 현재 정적 문자열 리스트만 지원한다. 각 문자열을 실제 C++ 메뉴 항목 생성 코드로
펼치며 런타임 목록 해석기를 넣지 않는다. 문자열 목록과 `{}` 자식 뷰는 함께 지정할 수 없다.
복합 항목은 명시적 MenuItem 자식으로 작성하며 MenuItem 안에는 소유한 Menu·ContextMenu를
둘 수 있다. 소유 팝업의 열기는 앱의 C++ 콜백이 담당하고 닫기는 소유 계층에 전파된다.

메뉴는 `compactItems`, `itemWidth`, `itemSpacing`, 네 방향 padding, `edgeMargin`,
`cornerRadius`, `menuOpacity`, `selectedIndex`, `autoCloseOnTrigger`,
`dismissOnGlobalPress`, `dismissOnGlobalContextRequest`, `showIconSlot`,
`onItemTriggered`·`onOpened`·`onClosed`를 지원한다. 항목은 `label`, `key` 또는
`shortcut`, `keyPlaceholder`, `iconName`, `enabled`, `compact`,
`state: .default/.selected/.inactive`, `selectionDirection: .auto/.right/.left/.up/.down`,
아이콘·단축키·화살표 표시, `expanded`, 크기와 `onClick`, `keepOpen` 또는
`closeOnTrigger`를 받는다. 구분선은 축·두께·여백·길이를 받는다.

항목 액션, 구분선 위치를 포함하는 `int` 메뉴 인덱스 콜백, 자동 닫기 순서로 실행한다.
자식 표시가 있는 항목은 기본적으로 열린 상태를 유지한다. 다른 C++ 이벤트와 같이
콜백이 트리를 제거하면 후속 디스패치를 중단하고, 팝업을 다시 연 경우 이전 활성화의
자동 닫기를 적용하지 않는다. 위·아래·Home·End·Tab 이동, 비활성 항목 건너뛰기,
스크롤·화면 경계 배치·입력 포착·바깥 클릭·Escape 닫기와 포커스 복귀는 iiGUI가 맡는다.

원본 재질의 불투명 바탕과 12% 코팅을 제공하지만 64 px 배경 흐림·방사형 악센트·그레인·
열림 스프링 모션은 아직 구현하지 않았다. 동적 객체 모델, 이름·payload를 가진 복수 이벤트,
전체 상속 속성·메서드와 일반 반응형 바인딩도 남아 있다. 이 타입들도 부분 이식 상태이다.

## 색상 모델과 여섯 네이티브 표면

`ColorPickerSurface`와 원본 인라인 타입 `ColorField`를 AOT로 생성한다. `model`에는 가져온
C++ 백엔드의 공개 `std::shared_ptr<lvrs::ColorPickerModel>` 멤버를 지정한다. 같은 멤버를 사용한
표면들은 동일한 C++ 모델을 공유하며 Core 변경 쿼리로 현재 색상과 선택점을 갱신한다.

```text
import vm ColorBackend
ApplicationWindow {
    HStack {
        ColorPickerSurface(model: vm.model, field: .Wheel)
        ColorField(model: vm.model, field: .SaturationBrightness)
        ColorPickerSurface(model: vm.model, field: .HueRail, width: 18)
    }
}
```

지원 입력은 `id`, `model`, `field`, `width`, `height`, `enabled`, `accessibilityName`,
`onColorEdited`이다. 콜백은 `ii::gui::WidgetColor`를 받는다. `field`는 `.Wheel`,
`.HueSaturation`, `.SaturationBrightness`, `.Spectrum`, `.HueRail`, `.BrightnessRail`이다.
표면은 자식을 받지 않으며 크기는 12..1024 논리 단위이다. 모델의 public 접근성과 정확한 C++
타입은 생성 코드를 컴파일할 때 검사한다. 메서드 호출이나 임의 표현식을 모델로 지정하지 않는다.

변경은 동일한 모델의 명시적 API를 통해 전달된다. 임의 C++ 대입을 자동으로 관찰하거나 모델
멤버의 포인터 재대입을 자동 연결하는 기능은 아직 없다. 명시적 `set_color_model`로 연결을
교체할 수 있다. 이것은 일반 반응형 언어 계약의 완료 선언이 아니다. 여섯 표면 예제는
LVRS의 `examples/NativeColorGallery`이다.

## 합성 ColorPicker

`ColorPicker(type: .Wheel, model: vm.model)`는 채널·숫자 입력·색상 도메인·비교 견본·
HEX·알파·최근 색상·액션을 함께 만드는 네이티브 C++ 호출을 생성한다. 모델은 선택 사항이며
생략하면 선택기마다 독립 모델을 소유하고 원본 기본색 `#874AFF`로 시작한다.
`.Wheel`, `.HueSaturation`, `.SaturationBrightness`, `.Grayscale`, `.RGB`, `.CMYK`를 지원한다.
`width`, `padding`, `enabled`, `showRecentColors`, `showActions`, `applyText`, `cancelText`와
정적 HEX 문자열 목록 `recentColors`를 지정할 수 있다. 최근 색상은 처음 10개만 사용한다.

`onColorEdited`, `onAccepted`는 `ii::gui::WidgetColor`를 받는 공개 C++ 메서드 참조이며
`onCanceled`, `onEyedropperRequested`는 인자 없는 메서드 참조이다. 입력은 Enter/포커스 손실에
확정하며 수정하지 않은 반올림 표시를 모델에 다시 쓰지 않는다. 적용·취소는 뷰를 닫지 않고
소유자 콜백으로 전달한다. 스포이트도 소유자가 문서 샘플링을 시작하기 위한 요청이다.

옵션은 생성 시 상수이며 일반 반응형 재연결은 아직 없다. C++의 `lvrs::ColorPicker` 핸들은
타입·폭·숨김·활성·최근 색상을 같은 마운트에서 바꿀 수 있다. 합성 모델 포인터의 변경은
재마운트한다. 원본 전체 시각·상속 계약의 완료를 뜻하지 않는다. 전용 설치 예제는
`examples/NativePickerGallery`이며 상세 계약은 LVRS의 `docs/native/ColorPicker.md`이다.

## 일반 콘텐츠 Popover

원본 Modal의 동적 콘텐츠 수명을 위한 iiGUI C++ 팩토리 경로도 추가했다. 현재는 빈
`Popover`를 AOT로 만든 뒤 C++에서 `set_popup_content`를 연결한다. 콘텐츠 위젯은 매번
열 때 만들고 닫으면 소유권을 해제하지만 바깥 LVUI 백엔드는 유지한다. 현재 콘텐츠의
조회는 `popup_content(local_id)`이며 이전 활성화 핸들은 재사용할 수 없다.
이제 `Modal { ... }`의 자식을 별도의 `ModalContentFactory`로 AOT 생성하며 전용 Modal
조합도 제공한다. 기본 헤더·아이콘·최대 세 액션과 가운데 배치는 LVRS가 구성한다.
`open: true`는 마운트 후 최초 개방한다. 자식 ID와 context menu 대상은 콘텐츠 내부 범위에서
해석하며 바깥 백엔드는 기존 스코프를 빌린다. 부모 백엔드의 재생성은 없다. 호출식 대신 메서드
참조를 쓰는 규칙도 같다. 초기 개방을 요구하는 중첩 생성 함수와 범위 간 target 참조는 진단한다.
전체 지원 입력과 남은 차이는 LVRS의 `docs/native/Modal.md`에 기록한다.
예제는 LVRS의 `examples/NativeActivationGallery`, 상세 계약은
`docs/native/ActivationContent.md`이다.

`ColorPickerButton`도 `FRAMEWORK LVRS`에서 자체 버튼 호출로 생성한다. `currentColor`의
6/8자리 HEX 또는 공개 공유 `model` 멤버 중 하나를 받으며, 생략하면 기본 보라색이다.
`buttonSize: .Small/.Medium/.Large` 또는 22/28/36, `showHueRing`, `enabled`,
`accessibilityName`, `onClick`·`onPressed`·`onReleased`·`onCanceled`를 지원한다.
이벤트는 메서드 참조이고 모델·초기색의 동시 지정, 호출식과 미지원 크기는 진단한다.
생성 옵션은 상수이며, 공유 모델의 색상 변경은 Core 쿼리를 통해 명시적으로 연결된다.
팝오버를 자동 소유하지 않는다. 예제와 미완료 계약은 LVRS의
`docs/native/ColorPickerButton.md`에 있다.

`Popover`는 한 일반 콘텐츠 View를 받는 레이아웃 외부 오버레이이다. 복수 콘텐츠는 하나의
스택으로 묶으며 별도로 소유한 Menu·ContextMenu·Popover도 둘 수 있다. 닫힌 상태에서도
콘텐츠와 스코프를 유지하고 열 때마다 같은 C++ 객체를 사용한다.

```lvui
Popover(id: "editor", padding: 12, margins: 8, closeOnEscape: true,
        closeOnPressOutside: true, onClosed: vm.closed) {
    ColorPicker(model: vm.model, onAccepted: vm.accepted, onCanceled: vm.canceled)
}
```

`width`, `height`, `padding`, `margins`, `cornerRadius`, `surfaceColor`, `surfaceOpacity`,
`modal`, `dim`, `focus`, `clip`, `closeOnEscape`, `closeOnPressOutside`, `accessibilityName`,
`onOpened`, `onClosed`를 생성 시 받는다. `surfaceColor`는 6자리 RGB HEX이며 코팅 알파는
`surfaceOpacity`이다. 두 이벤트는 인자 없는 메서드 참조이다. 현재 초기 열림·좌표·동적 바인딩
입력은 지원하지 않으며 마운트 후 C++ 위젯 핸들의 `open_for`·`open_at`·`close_popup`을 쓴다.
자식 Escape 처리, 포커스 복귀, 외부 누름의 모달/비모달 전달은 iiGUI가 담당하고 모델의
확정·복원은 소유자 콜백이 결정한다. OS 팝오버를 만들지 않는다. 원본 재질·모션·상속 계약은
부분 구현이며 상세 계약과 실행 예제는 LVRS의 `docs/native/Popover.md`에 있다.

## Alert와 AlertButton

Alert은 원본의 자동/2/3개 액션과 1·2개 가로 배치/3개 세로 배치를 C++로 조합한다.
`description`은 `message`, `button1Text/2Text/3Text`는 `primaryText/secondaryText/tertiaryText`,
`imageSource`는 `appIconSource`의 별칭이다. 원래 이름과 별칭을 함께 지정하면 진단한다.
`buttonCount`는 정수이며 2·3 외에는 텍스트 유무에 따른 자동 선택이다.

```lvui
import vm AlertBackend
ApplicationWindow {
    AlertButton(text: "Run", method: vm.run, onClick: vm.afterRun)
    Alert(id: "confirmation", open: true, title: "Native Alert",
          description: "Choose an action.", buttonCount: 3,
          primaryText: "Apply", secondaryText: "Discard", tertiaryText: "Later",
          imageSource: "alert-file-text.svg", button1Method: vm.apply,
          button2Method: vm.discard, onPrimaryClicked: vm.afterApply)
}
```

`method`·`button1Method/2Method/3Method`는 지연 메서드 참조이다. 생성한 C++의 `requires`로
`const lvrs::ButtonEvent&` 인자를 받는 호출을 먼저 선택하고, 없으면 인자 없는 호출을 선택한다.
이벤트에는 실제 소스 Widget과 `clicked` trigger가 있다. 메서드 뒤 클릭 통지를 실행하며
소유 View가 제거되면 이전 세대의 후속 통지는 실행하지 않는다. `vm.method()`는 오류이다.
부모 backend는 View 활성화 때 한 번 생성되고 클릭·재개방·그리기에서 재사용한다.

AlertButton은 `dialogStyle`, 다섯 톤, enabled, 선택적 height/horizontalPadding/cornerRadius,
textColor/primaryColor와 누름·해제·취소 콜백을 지원한다. Alert의 `open: true`는 마운트와
backend 구성 뒤 최초 개방한다. 닫힌 Alert은 정적 위젯을 유지하며 콘텐츠를 새로 생성하지 않는다.
`minWidth/maxWidth/preferredWidth/sidePadding/cardCornerRadius/shapeStyle`, 액션별 enabled,
secondaryDestructive, 아이콘 크기·프레임·색상, backdrop/card 색상과 glass 설정을 전달한다.
색상 리터럴은 기존 6/8자리 RGB/RGBA HEX 입력이다. 임의 자식 View는 받지 않는다.

이미지는 두 원본 Alert SVG 이름·기존 QRC 리터럴 또는 C++ 스코프의 불변 RasterImage 멤버이다.
빈 문자열은 원본 대체 사각형 표시이며 알려지지 않은 URL·파일은 컴파일 진단한다.
그림자와 420ms OutBack/150ms InCubic 카드 모션도 실제 Metal/Core 프레임으로 동작한다.
`shadowEnabled/shadowColor/shadowBlurRadius/shadowOffsetX/shadowOffsetY`와
`motionEnabled`를 컴파일하며, `motion: vm.motion`은 `std::shared_ptr<lvrs::Motion>`
멤버를 빌린다. 실행 중 speed·enabled·reduced_motion을 바꾸어도 같은 객체를 사용한다.
현재 효과와 합성의 실제 지원은 Metal 전용이다. 일반 반응형 재바인딩·다른 재질 백엔드와
전체 상속 계약은 [LVRS Alert 문서](../../LVRS/docs/native/Alert.md)에 남은 작업으로 기록한다.


## Modal의 정적 외곽과 프레임 전환

Modal의 제목·아이콘·액션은 마운트 시 한 번 생성한다. AOT 자식 팩토리는 본문만
정적 외곽의 앵커에 넣으며 닫는 즉시 해제한다. 외곽은 같은 ID를 유지하며 420ms
OutBack·150ms InCubic으로 전환한다. 퇴장 중 다시 열면 현재 진행 값에서 새 본문을
생성한다. `motionEnabled`와 `motion: vm.motion`을 네이티브 설정으로 생성하며
인자 호출이나 알 수 없는 스코프 참조는 컴파일 진단이다. `frameColor`의 RGBA alpha도
전달한다. [Modal 계약](../../LVRS/docs/native/Modal.md)에 실제 수명·검증 범위를 명시한다.

## 표의 셀·행·헤더

세 시각 타입은 정적 셀/열 데이터, 기하, 편집·submit·검증 거절과 헤더 활성화를 생성한다.
`onInputEdited/onInputSubmitted`는 String, `onInputRejected`는 두 String,
`onCellInputEdited/onCellInputSubmitted`는 열 인덱스와 String,
`onColumnClicked`는 열 인덱스와 `const lvrs::TableHeaderColumn&`를 전달한다.
`valueValidator`는 scope backend의 public `TableValidator` 값 참조이다.
이 standalone 구성 요소만으로 전체 Table 데이터 모델을 자동 생성하지 않는다.
아래의 `Table`은 C++ 모델에 연결하며 임의 role/delegate의 전체 이식은 남아 있다.
[네이티브 표 계약](../../LVRS/docs/native/Tables.md)에 지원 범위와 예제를 기록한다.

## 모델에 연결된 Table

`Table(model: vm.model, width: 660, height: 300)`은 public C++
`shared_ptr<lvrs::TableModel>` 멤버를 scope에서 빌려 같은 네이티브 Table 생성기로 전달한다.
선택 모드, 표시 크기/색, 수직 viewport, 정렬과 타입 편집 콜백을 AOT C++로 생성한다.
행/열/값은 C++ 모델 API로 관리한다. 임의 rows/role/delegate 배열과 모델 자동 재바인딩,
전체 상속 API는 아직 미지원이다. `NativeDataTableGallery`와 새 검사 코드는 빌드 단계이며
실행 검증은 최종 일괄 단계까지 유보한다. 자세한 범위는 LVRS의
`docs/native/TableView.md`에 기록한다.

`Table`은 `resizeHandlesVisible`, `columnResizeHandleWidth/rowResizeHandleHeight`,
`resizeHandleColor/resizeHandleHoverColor`, `structureControlsVisible`,
`addRowControlsVisible/addColumnControlsVisible`, `structureGutterWidth/Height`,
`deleteContextMenuEnabled`, `selectOnContextClick`, `clipboardEnabled`를 AOT 옵션으로 받는다.
크기 조절·삽입 영역·삭제 메뉴와 범위 붙여넣기는 같은 C++ 컨트롤러를 사용한다.
`onRangeValuesChanged`는 `lvrs::TableRange`, `onColumnResized/onRowResized`는 인덱스와
double 치수를 전달한다. 이 추가 검사도 최종 일괄 실행 단계까지 유보한다.

`Table`의 보관형 viewport는 모델의 전체 폭과 높이를 두 축의 논리 범위로 사용한다.
`fitColumnsToViewport: false`는 명시적인 모델 열 폭을 그대로 사용한다. 가로 휠과
Shift+세로 휠, 키보드 셀 드러내기와 화면 밖 열 편집은 같은 C++ Table 경로를 사용한다.
헤더·resize·삽입·우클릭도 같은 논리 X 좌표로 동작한다. AOT는 가로 스크롤을 위한
소스 평가기나 행/셀마다의 위젯 생성기를 추가하지 않는다. 네이티브 Table의 명시적인
`set_scroll_position`, `set_horizontal_scroll_offset`, `reveal_cell`은 C++ API이다.
새 예제·입력·픽셀·설치 소비자 실행 검증은 최종 일괄 단계까지 유보한다.
