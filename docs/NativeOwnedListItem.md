# LVUI 선언과 iiGUI 목록 항목

기본 `ListItem`의 상태·배치·접근성·그리기와 입력 표면은 `iiGUI/ListItemModel.h`,
`iiGUI/ListItemDrawing.h`, `iiGUI/NativeListItemView.h`에 있다. LVUI 어댑터는
디자인 값·아이콘·콜백을 전달하며 일반 [목록 화면](NativeOwnedListView.md)도 같은
행 배치·그리기를 사용한다. `LVUI/ListItem.h`의 구형 행 계산 모델은 제거했고
가상 목록 호스트와 복합 항목의 내부 컨트롤은 아직 LVUI에 남아 있다. Qt, QML, LVRS 위젯에 의존하지
않는다. 앱은 `ListItemInput`과 `NativeListItem::create()`로 개별 행을 만들거나
다음과 같이 선언할 수 있다.

```text
import vm RowModel
Window {
    ListItem(type: .Navigation, label: vm.label, description: vm.description,
             selected: vm.selected, onClick: vm.activate)
}
```

현재 독립 선언은 `.Mini`, `.Detail`, `.Navigation`, `.Toggle`, `.Checkable`, `.Action`, `.ActionGroup`, `.Stepper`, `.Select`, `.InlineEdit`, `.DetailActions`, `.DetailQuantity`, `.DetailSettings`, `.Task`, `.Resource`, `.Media`, `.Form`과 같은 소문자 표기를 지원한다.
기본 폭과 높이는 각각 170×20, 194×44, 탐색·토글·체크의 280×44와
액션·스테퍼·선택·인라인 편집 유형의 400×44이며 명시적인 `width`와 `height`로
바꿀 수 있다. 이름이나 직접 제공한 래스터, 교체 가능한 아이콘 공급자로 아이콘을
선택하고, 선택·비활성·호버·누름·포커스 색상과 간격을 조정할 수 있다. 아이콘은
기본적으로 비어 있다. 일반 행은 높이가 32 이상인 경우 설명을 별도 줄에 표시한다.

iiGUI를 직접 사용하는 경우 `ii::gui::ListRowConfiguration`의 색상과 치수,
`ListItemInput`의 아이콘·화살표 이미지 또는 공급자를 앱이 제공한다. LVUI와 iiGUI의
값 타입 이름은 두 네임스페이스를 함께 사용하는 기존 C++ 소비자와 충돌하지 않는다.
macOS arm64에서 기본·복합 행과 목록 화면·패널, 편집·작업·미리보기 및 사용자
콘텐츠의 헤드리스 검사를 통과했다. 실제 창·GPU와 다른 운영체제 검증은 포함되지 않는다.

`selected`는 앱이 관리하며 클릭은 `onClick`을 전달한다. `enabled`, `interactive`,
`selectable`, `visible`이 입력을 제어한다. 갱신 가능한 속성은 반응형으로 연결된다.
잘못된 설정이나 아이콘은 거부하고 마지막 정상 설정을 유지한다. 렌더링과 접근성에
사용하는 텍스트는 UTF-8 경계에 맞춰 각각 최대 8,192바이트로 제한하며 원본 설정은
유지한다. 앱이 소유한 상태는 컴포넌트가 되돌리지 않는다.

C++의 `ListItemInput::accessibility_description`은 표시 텍스트와 별도로 접근성 설명을
지정할 때 사용하며, 비어 있는 선택 값은 원래 설명을 사용한다. 고급 조합은
`custom_content`를 켜고 `NativeListItem::set_content_painter()`로 콘텐츠 그리기를
제공할 수 있다. 이때 행의 배경과 포커스는 기본 행이 그리고, 콜백은 전달받은
`WidgetDrawing`만 채워야 한다. 콜백에서 위젯을 조회하거나 변경해서는 안 된다.

iiGUI의 `BasicNativeListItemComposite`는 주입된 토글과 체크박스 구현을 조합해 토글과
체크 유형을 제공한다. 토글은 기본적으로 켜져 있고 체크는 기본적으로 꺼져 있으며,
명시적인 `checked`로 초기값을 정할 수 있다. 값이 바뀌면 `checked_changed`와
`edited("checked", value)`를 순서대로 전달한다. 앱이 현재 값을 반응형으로 다시
제공하는 경우 이후 이벤트를 유지한다. 각 컨트롤의 활성 여부와 표시 여부는 별도로
설정할 수 있다. 선언에서는 `checked`, `toggleEnabled`, `selectionEnabled`, `showToggle`, `showSelection`과
`onCheckedChanged`·`onEdited`를 사용한다. `onEdited`의 값은 `lvui::ListItemValue`다.

선언형 복합 항목의 내부 컨트롤은 창의 정적 또는 반응형 `accent`를 상속한다.
토글·체크·스테퍼, 선택기와 메뉴, 세그먼트, 텍스트 입력, 액션에 각 상태의 색상을
적용한다. 앱이 행의 색상을 직접 지정한 값은 행에 그대로 우선 적용하며 내부의 별도
컨트롤 색상으로 전파하지 않는다. C++ 직접 조합은 `ListItemCompositeInput::accent`에
`WindowAccentPalette`를 선택적으로 전달한다. 미지정 시 기존 컨트롤 기본값을 유지한다.
색상 채널이 유한한 0–1 범위를 벗어나면 이전 입력을 보존하고 갱신을 거부한다.
색상만 바뀔 때는 열린 선택기와 더보기·중첩 메뉴, 사용자 선택과 포커스, 입력 중인
초안·문자 조합을 유지한다. 앱이 `ListItemSegmentsBinding`으로 조합한 iiGUI 세그먼트의 C++ 입력에서도 선택적
`ListItemSegmentsInput::accent`를 사용할 수 있다. 색상을 생략하거나 다시 비우면
기본 팔레트를 사용하며 선택·포커스와 위젯 수명을 유지한다. 사용자 정의 View는
스스로 스타일을 지정한다.

C++의 `.action`과 `.action_group` 유형은 LVUI `ListFooter` 슬롯으로 기본·보조·추가
액션을 조합한다. `ListFooterSlot`에 표시, 활성 여부, 아이콘 및 교체 가능한 명령을
전달하며 `action_triggered`는 `primary`, `secondary`, `more`와 현재 슬롯을 전달한다.
명령 실행 중 항목이 제거되면 오래된 이벤트는 전달하지 않는다. 선언에서 `primaryAction`, `secondaryAction`, `moreMenu`는 문자열이나
`ListFooterSlot`을 받으며 `onActionTriggered`로 결과를 전달한다.
이 콜백의 두 번째 인자는 슬롯 설정이다. 액션을 누른 시점의 값은 별도의
`onActionValuesTriggered(name, values)`로 받을 수 있다. `values`는
`lvui::ListItemActionValues`이며 두 입력 문자열, 체크, 수량, 선택·단위·세그먼트
인덱스를 포함한다. C++에서는 `action_values()`로 현재 값을 읽을 수 있다. 기본·보조
액션의 이름은 `primary`·`secondary`이고 값 콜백에서 더보기 이름은 원본과 같은
`menu`다. 기존 슬롯 콜백의 `more` 이름은 호환성을 위해 유지한다. 값은 슬롯 콜백
이전에 복사하며 그 콜백이 항목을 제거하거나 상태를 바꾸면 오래된 값 이벤트는
전달하지 않는다. 원본처럼 버튼, 더보기, 메뉴 항목 선택, 입력 제출을 하나의
콜백에서 처리하려면 `onActionEvent(name, payload)`를 사용한다. 페이로드는
`lvui::ListItemActionPayload`이며 다음 세 가지 중 하나다.

- `primary`, `secondary`, `menu` 및 사용자 액션: `ListItemActionValues` 값 스냅샷.
  사용자 뷰에서 페이로드를 전달한 경우 `custom_payload`에 함께 담긴다.
- `menuItem`: `ListItemMenuSelection`의 `index`와 선택한 `ListItemOption item`.
- `inputSubmitted`: `ListItemInputSubmission`의 `field`(`inputText1` 또는
  `inputText2`)와 제출한 `text`.

C++에서는 `std::get_if` 또는 `std::visit`으로 페이로드를 구분하고
`NativeListItemCompositeCallbacks::action_event`로 같은 이벤트를 받는다. 기존
`onMenuItemTriggered`, 입력 제출 콜백과 `onActionValuesTriggered`도 유지한다.
해당 기존 콜백이 먼저 실행되며 그 과정에서 항목이 제거되거나 상태가 바뀌면 통합
이벤트는 보내지 않는다. 기존 `onMenuItemTriggered`의 항목 값은 문자열과 활성
상태이며, 확장 메뉴 항목은 아래의 `ListItemMenuSelection`으로 추가 정보를 전달한다.

`.DetailActions`는 제목과 최대 두 줄 설명, 앞 아이콘과 북마크 아이콘, 날짜와 두
메타데이터 열, 상태와 세 액션을 세로로 배치한다. 기본 자연 크기는 400×120이며
설명이나 메타데이터, 버튼을 숨기면 높이가 줄어든다. `width`, `height`,
`detailHorizontalPadding`, `sectionSpacing`, `contentSpacing`, `iconSize`, `actionWidth`로
배치를 조절한다. `showLeadingIcon`, `showBookmark`, `showDescription`, `showMetadata`와
기존 세 액션 표시 설정은 반응형으로 바뀐다. `dateText`, `metadata1`, `metadata2`,
`statusText`, `bookmarkIconName`으로 내용을 제공한다. 고정 `.DetailActions` 선언에서
생략한 설명은 `Supporting text`, 기본 액션은 `Open`과 `Edit`다. 선행 아이콘은 지정하지
않으면 `nodesfolder`를 사용한다. C++의 `ListItemCompositeInputValue<Image>`나 동적으로 바꾸는
`type`은 기존 입력 기본값을 유지하므로 원하는 설명과 액션 텍스트를 명시한다.
C++에서는 `ListItemCompositeBinding`으로 iiGUI의 복합 항목 구현을 조합해 생성한다. 기본 가상 목록 행은 단순 행을
그리므로 목록 안에 복합 항목이 필요하면 행 대리자에서 해당 컴포넌트를 구성해야 한다.
표시 중인 설명·메타데이터·상태는 행의 접근성 설명에도 전달하고, 숨긴 설명과
메타데이터는 제외한다. 운영체제별 보조 기술에서의 실제 읽기 결과는 아직 검증하지 않았다.

```text
import vm ProjectModel
Window {
    ListItem(type: .DetailActions, label: vm.title, metadata1: vm.kind,
             moreItems: ["Rename", "Delete"], onMenuItemTriggered: vm.menuAction)
}
```

`moreItems`는 문자열 배열 또는 C++의 `std::vector<lvui::ListItemOption>`을 받는다.
항목은 최대 400개이며 각 `ListItemOption`은 문자열과 활성 여부를 가진다. 더보기를
누르면 기존 `onActionTriggered("more", slot)` 뒤에 메뉴가 열리고, 활성 항목을
고르면 `onMenuItemTriggered(int index, const lvui::ListItemOption& item)`을 전달한다.
목록이 비어 있으면 메뉴를 열지 않는다. 목록이 바뀌면 열린 메뉴를 닫고 다음번에
새 항목을 구성하며, 이전 항목에 대한 늦은 콜백은 무시한다. 긴 설명이
줄바꿈 한도를 초과하면 안전하게 한 줄로 생략한다. 실제 플랫폼에서의 픽셀 일치와
보조 기술 동작은 별도로 검증해야 한다.

#### 확장 더보기 메뉴

`moreItems`에 반응형 C++ `std::vector<lvui::ListItemMenuEntry>`를 전달하면 구분선,
아이콘, 단축키, 선택·비활성 상태와 중첩 메뉴를 사용할 수 있다. 기존 문자열 배열과
`std::vector<ListItemOption>` 입력도 그대로 사용할 수 있다. C++의 직접 입력에서는
`ListItemCompositeInput::menu_entries`를 사용하고, 이때 기존 `more_items`는 비워
두어야 한다. 두 입력을 동시에 지정하면 거부한다.

```text
import vm ProjectModel
Window {
    ListItem(type: .ActionGroup, label: "Project", moreItems: vm.menu,
             onMenuSelectionTriggered: vm.menuSelected)
}
```

각 `ListItemMenuEntry`의 `label`, `shortcut`, `icon_name`, `accessibility_name`,
`enabled`, `visible`, `state`, `direction`, `expanded`, `children`으로 항목을
구성한다. `show_chevron`은 화살표를 숨겨도 실제 하위 메뉴 탐색은 유지하며,
`has_child_items`는 아직 자식 데이터를 넣지 않은 항목에도 하위 표시를 지정할 수
있다. 자식이 존재하는데 `has_child_items`를 `false`로 설정하면 입력을 거부한다.
`divider = true`인 C++ 항목은 구분선이며 자식이나 표시·명령 설정을 가질 수 없다.
확장 메뉴의 아이콘 영역은 기본적으로 표시하고 단축키는 공백이 아닌 값이 있을 때
표시한다. `show_shortcut`과 `show_icon_slot`으로 각각 재정의할 수 있다. 기존 단순
메뉴는 이전처럼 아이콘 영역을 숨긴다. 아이콘은 목록 항목의 `iconSource`와
`iconRevision`을 사용한다. 메뉴를 열 때 아이콘 해석이 실패하면 이전 메뉴 세대를
보존하고 `failure()`에 오류를 기록한다. 정상 입력으로 변경하면 복구할 수 있다.
명시된 선택 상태는 키보드 탐색 중에도 보존된다. 활성 항목과 구분선은 키보드 탐색에서
서로 구별하며, 자식 메뉴는 기존 [네이티브 메뉴](NativeMenu.md)의 포인터·키보드
탐색과 상위 메뉴 닫기 동작을 사용한다.

항목의 `payload`에는 `std::shared_ptr<const std::any>`로 앱 소유의 자료를 담을 수
있고, 선택 시 전체 항목과 자료가 유지된다. 변경을 알리려면 새 불변 값이나 새 핸들을
발행한다. 선택 사항인 `ListItemMenuCommand`는 선택 시 `ListItemMenuSelection`을
받는다. `close_on_trigger`를 지정하지 않으면 일반 항목은 메뉴를 닫고 자식이 있는
항목은 연 상태를 유지한다. 값을 명시하면 그 설정을 따른다.

`onMenuSelectionTriggered(selection)`과 `onActionEvent("menuItem", payload)`의
`ListItemMenuSelection`은 최하위 `index`, 기존 호환용 `item`, 루트부터의 0 기반
`path`와 원본 확장 `entry`를 제공한다. 기존 문자열·`ListItemOption` 메뉴는 `path`가
비어 있고 `entry`가 없다. `onMenuItemTriggered`도 각 단계의 인덱스와 문구·활성
상태를 계속 받는다. 실행 순서는 항목 명령, 기존 메뉴 콜백, 상세 선택 콜백, 통합
액션 콜백이며 앞 단계에서 항목이 제거되거나 갱신되면 오래된 후속 콜백을 생략한다.
최대 항목 수는 모든 하위 메뉴를 합쳐 400개, 최대 깊이는 16단계다. 문구는 각
8192바이트, 아이콘 이름은 256바이트, 전체 문구는 1MiB로 제한하며 유효한 UTF-8을
요구한다. 입력 변경이나 아이콘 개정은 열린 메뉴를 닫고 다음 표시 때 새 세대를 만든다.
임의 QML 객체의 자동 변환과 메뉴 항목 사용자 View는 제공하지 않는다.

`.stepper` 유형은 LVUI `NativeStepper`와 [수량 모델](ListItemQuantity.md)을 조합한다.
기본 수량은 1, 최소값은 0, 최대값은 양의 무한대, 증분은 1이다. 수량과 범위,
증분을 변경할 수 있고 비활성·숨김 상태나 범위 끝에서는 값 변경 이벤트가 발생하지 않는다.
변경 시 `quantity_changed`와 `edited("quantity", value)`를 차례로 전달한다. 선언에는
`quantity`, `minimumQuantity`, `maximumQuantity`, `stepSize`, `quantityWidth`, `quantitySpacing`,
`stepperEnabled`, `showStepper`, `onQuantityChanged`, `onEdited`를 사용한다.
`showStepper: false`는 조절 버튼만 감추고 현재 수량은 계속 표시한다.
범위만 바뀌면 현재 수량을 새 범위로 제한하며, 명시 수량이 범위 밖이거나 잘못된
설정은 이전 상태를 유지한다. 앱이 현재 수량을 반응형으로 되돌려 제공해도 편집
이벤트는 전달된다. `stepperArrow`는 `.UpDown`, `.Up`, `.Down`을, `stepperTone`은
기존 스테퍼의 톤을 받는다. 비활성 톤이나 허용되지 않은 방향은 수량을 바꾸지 않는다.

`.select` 유형은 LVUI의 [선택기](NativeOwnedListItemSelector.md)를 사용한다. `selector`에는
문자열 목록 또는 C++의 `ListItemSelectorConfiguration`·`ListItemOption` 목록을 제공한다.
`selectorIndex`, `selectorWidth`, `selectorEnabled`, `showSelector`와
`onSelectorIndexChanged`로 상태를 제어한다. 변경 시 `selector_index_changed` 다음에
`edited("selectorIndex", value)`를 전달하며 편집 값은 `std::int64_t`다. 빈 목록은
기본 문구를 표시하고, 비활성 항목은 선택하지 않는다.

`.inline_edit` 유형은 LVUI 입력 필드와 기본 액션을 함께 표시한다. 입력의 초기값은
`Value`이며 `inputText1`, `inputPlaceholder1`, `inputWidth`, `inputHeight`,
`inputEnabled1`, `inputReadOnly1`, `showInput1`로 바꿀 수 있다. `onInputTextChanged`와
`onEdited`는 확정된 문자열을 전달하며 `onInputSubmitted`는 제출 시 현재 문자열을
받는다. 한글 등 조합 입력은 확정 전에는 편집 이벤트를 보내지 않고, 다른 속성을
갱신해도 진행 중인 조합을 유지하며 조합이 끝나기 전의 제출은 무시한다. 행이
콜백 중 제거되면 이후 편집 이벤트는 전달하지 않는다. 숨긴 선택기는 필요할 때까지
내부 메뉴를 만들지 않는다. 기본 액션은 기존 `primaryAction`과
`onActionTriggered`를 사용한다.

`.DetailQuantity`는 헤더의 형식 선택기, 수량과 스테퍼, 별도의 단위 선택기, 하단
토글과 기본 액션을 표시한다. 기본 자연 크기는 400×118이며 표시 여부에 따라
높이가 달라진다. 기존 `selector`·`selectorIndex`와 별개로 `unitSelector`,
`unitIndex`, `unitSelectorWidth`, `unitSelectorEnabled`, `showUnitSelector`를 사용한다.
`quantityLabel`과 `optionLabel`은 각 행의 설명을 지정한다. 선택이 바뀌면
`onUnitIndexChanged(int)` 다음에 `onEdited("unitIndex", std::int64_t)`를 전달한다.
선언에서 비어 있는 형식·단위 선택기의 기본 문구는 `Format`과 `Units`이며 앱이 제공한
`ListItemSelectorConfiguration::placeholder`는 그대로 유지한다. C++ 입력의 일반
선택기 기본 문구는 `Option`이므로 필요한 문구를 직접 설정할 수 있다.

`.DetailSettings`는 헤더의 토글, 모드 선택기, 범위 세그먼트, 상태와 Reset·Apply
액션을 표시한다. 기본 자연 크기는 400×159이다. `modeLabel`, `scopeLabel`,
`segments`, `segmentIndex`, `segmentWidth`, `segmentsEnabled`, `showSegments`로
내용을 바꾼다. 세그먼트 변경은 `onSegmentIndexChanged(int)` 다음에
`onEdited("segmentIndex", std::int64_t)`를 전달한다. 기본 세그먼트는 `Original`,
`Edited`이며 선언에서 비어 있는 모드 선택기는 `Automatic`을 표시한다. 두 상세 유형의
토글은 기본적으로 켜져 있고, 설정형 액션의 표시 순서는 Reset 다음 Apply다.
상세 유형의 기본 설명과 액션 문구는 정적 선언과 동적 유형 전환에 적용된다.
C++ 입력을 직접 만들 때는 원하는 문구를 명시한다.

```text
import vm ExportModel
Window {
    ListItem(type: .DetailQuantity, label: "Size", unitSelector: ["px", "cm"],
             quantity: vm.amount, unitIndex: vm.unit, onUnitIndexChanged: vm.unitChanged)
}
```

`segments`는 문자열 배열이나 C++의 `std::vector<ListItemSegment>`를 받는다.
각 항목은 텍스트·활성 여부와 선택적인 `ListFooterCommand`를 가지며, 이미 선택된
항목을 다시 눌러도 명령은 실행하지만 변경 이벤트는 중복하지 않는다. 좌우 키는
비활성 항목을 건너뛴다. 표시 폭의 합은 8,192 이하여야 하고 항목은 최대 400개다.
항목을 콜백 중 교체하면 콜백 이후 새 위젯 세대를 구성한다. 항목에는 라디오 버튼
접근성 역할을 제공하지만 실제 운영체제 보조 기술 검증은 아직 수행하지 않았다.

`.Task`는 체크박스와 우선순위 선택기가 있는 헤더, 최대 두 줄 설명, 세 개의
메타데이터와 수량·스테퍼·액션을 표시한다. 기본 자연 크기는 400×135이며 숨긴
섹션에 따라 높이가 줄어든다. `checked`, `selectionEnabled`, `showSelection`,
`selector`, `selectorIndex`, `showSelector`, `dateText`, `metadata1`, `metadata2`,
`showMetadata`, `quantity`, `quantityLabel`, `showStepper`와 기존 액션 설정으로
조작한다. 기본 체크는 꺼져 있고, 빈 선언형 선택기는 `Priority`, 기본 액션은
왼쪽부터 `Assign`, `Done`을 표시한다. 체크·선택·수량 변경은 각각의 변경 콜백과
`onEdited`에 전달되며, 버튼의 의미상 인덱스는 기본 액션 0, 보조 액션 1이다.
동적으로 `.Task`로 바꿀 때도 기본 문구가 갱신된다. C++에서 직접 입력을 만들면
선택기와 액션의 문구를 직접 지정한다. 배치만을 쓰는 소비자는 `ii::gui::BasicListItemTask`의
크기와 영역 계산을 사용할 수 있다.

`.Resource`와 `.Media`는 48×48 미리보기를 가진 복합 항목이다. 리소스는 기본
400×129이며 최대 두 줄 설명, 날짜·메타데이터, 헤더의 더보기 메뉴와 하단의
`Reveal`·`Open`을 표시한다. 미디어는 기본 400×113이며 한 줄 설명, 재생 시간과
미디어 정보, 세그먼트, 상태와 `Stop`·`Play`를 표시한다. 표시 설정에 따라 높이는
변한다. `previewSize`, `showPreview`, `previewIconName`, `durationText`, `mediaDetail`,
기존 메타데이터·액션·세그먼트 속성으로 구성한다. 세그먼트와 더보기 메뉴는 위에서
설명한 변경 이벤트와 수명 규칙을 따른다. 리소스의 더보기는 기존
`onActionTriggered`에 `more`, 값 콜백에는 `menu`를 전달한다.

`previewSource`는 앱이 제공하는 불변 `std::shared_ptr<const ii::gui::RasterImage>`
핸들이거나 화면 서비스 또는 개별 `imageLoader`가 해석하는 파일·URL 문자열이다. 가로세로 비율을
유지해 미리보기 영역 안에 맞춘다. 이미지가 없으면 `previewIconName`(기본 `nodesfolder`)을
기존 `iconSource`와 `iconRevision`을 통해 해석한다. 아이콘 해석이 실패하면 이전
상태를 보존하며 `showPreview: false`는 해석과 그리기를 생략한다. 미리보기 배경은
기본 스타일 토큰 `panel_surface_alt`를 사용한다. 유형을 동적으로 변경해도 각
기본 액션 문구가 바뀐다. C++ 입력은 원하는 문구를 직접 지정한다. 독립 배치 모델은
`ii::gui::BasicListItemPreviewRow`다. 로더는 [이미지 로딩](NativeImageLoading.md)을 따르며 실제
GPU·OS별 표시 결과는 별도다.

`previewComponent`에는 리소스·미디어 항목의 기본 이미지를 대신할 `View`를 전달할
수 있다. 사용자 뷰의 자연 폭과 높이를 사용하고 이후 크기 변경도 반영한다.
`previewSize`를 지정하면 정사각형 영역으로 재정의하며, 기본 이미지에 크기를
지정하지 않은 경우에는 48을 사용한다. `showPreview`, 항목의 활성 상태와 유형
변경에 맞춰 표시·입력을 조절하고 영역 밖을 자른다. 사용자 뷰가 있을 때는 기본
이미지나 아이콘을 해석하거나 그리지 않는다. `footerComponent`와 함께 사용할 수
있으며 C++에서는 `NativeListItemCompositeViewsValue<Builder, Factory, Image>::preview`와 선택적인
`ListItemCompositeInput::preview_size`를 사용한다. 독립 배치 모델의
`ListItemPreviewRowConfiguration::preview_width`·`preview_height`도 각각 선택적으로
지정할 수 있다.

`leadingComponent`는 `Navigation`, `Toggle`, `Action`, `ActionGroup`, `Stepper`,
`Select`, `InlineEdit`, `DetailActions`, `DetailSettings`의 앞쪽 아이콘을 하나의
사용자 `View`로 대체한다. 자연 폭·높이와 자식의 크기 변경을 반영하며, 명시적인
`iconSize`는 해당 영역을 정사각형으로 재정의한다. `showLeadingIcon`으로 표시를
제어하고 비활성 상태를 자식 입력에 전달하며 영역 밖을 자른다. 행이 좁으면 배치
모델이 사용 가능한 선행 공간을 먼저 할당하고, 남은 공간에서만 텍스트를 그린다.
C++에서는 `NativeListItemCompositeViewsValue<Builder, Factory, Image>::leading`, 선택적인
`ListItemCompositeInput::leading_size`와 독립 `ListItemConfiguration`의
`reserved_leading`·`reserved_leading_height`를 사용할 수 있다. 복합 항목의 독립 배치 모델은
`ListItemDetailActionsConfiguration`과 `ListItemDetailControlsConfiguration`의 선택적
`leading_width`·`leading_height`를 사용한다. 자연 크기가 지원 상한을 넘으면 전체
항목 높이가 8192를 넘지 않게 표시 영역을 제한한다.

`trailingComponent`는 `Navigation`, `Toggle`, `Checkable`, `Action`,
`ActionGroup`, `Stepper`, `Select`, `InlineEdit`의 오른쪽 기본 컨트롤을 사용자
`View`로 대체한다. 폭과 높이는 기본적으로 자식의 자연 크기를 따르고, 자식이
변경되면 다시 측정한다. 각 축은 `trailingWidth`와 `trailingHeight`로 별도 지정할
수 있다. 행 높이를 지정하지 않으면 자식에 맞게 커지며, 고정 높이인 경우 영역 밖의
표시와 입력은 자른다. 비활성 상태는 자식에게 전달한다. `Checkable`의 왼쪽
체크박스는 계속 동작한다. 단순 행에서 다른 유형으로 전환해도 뷰 상태는 유지되며
복합 항목에서는 기존 컨트롤을 사용한다. 이미 열린 기본 메뉴는 대체 시 닫힌다.
C++에서는 `NativeListItemCompositeViewsValue<Builder, Factory, Image>::trailing`과 선택적인
`ListItemCompositeInput::trailing_width`·`trailing_height`를 사용한다.

`.Form`은 제목·설명·분류 선택기, 두 개의 입력 행, 토글과 `Cancel`·`Save` 액션을
표시한다. 기본 자연 크기는 400×170이고 빈 선언형 선택기는 `Category`를 표시한다.
`fieldLabel1`, `fieldLabel2`, `inputText1`, `inputText2`, `inputPlaceholder1`,
`inputPlaceholder2`, `inputEnabled1`, `inputEnabled2`, `inputReadOnly1`, `inputReadOnly2`,
`showInput1`, `showInput2`와 공통
`inputWidth`, `inputHeight`로 각 입력을 구성한다. 두 필드의 값과 조합 상태는
독립적이므로 다른 필드를 갱신해도 진행 중인 한글 등의 조합을 보존한다. 첫 번째
필드는 기존 `onInputTextChanged`·`onInputSubmitted`, 두 번째 필드는
`onInputText2Changed`·`onInput2Submitted`를 사용한다. `onEdited`는 각각
`inputText1`·`inputText2`와 문자열 값을 전달한다. 각 입력의 접근성 이름은 해당
필드 레이블이다. C++ 소비자는 `input_field()`·`input_field2()`와
`input_text()`·`input_text2()`로 접근하고 `ii::gui::BasicListItemForm`으로 독립 배치를 계산한다.
제출은 LVUI의 필드별 콜백과 통합 `inputSubmitted` 이벤트로 전달된다.

`footerComponent`에는 하나의 선언형 `View`를 전달해 목록 항목 아래에 사용자 정의
내용을 조합할 수 있다. 자식의 자연 높이와 `sectionSpacing`만큼 자동 높이가 늘어나며,
고정 높이나 상위 레이아웃이 할당한 높이에서는 기존 본문과 하단 뷰가 겹치지 않게
배치한다. 자식의 반응형 크기 변경과 처음에는 비어 있던 자식이 나중에 나타나는
경우도 반영한다. `Navigation`부터 `Form`까지 표시하며
`Mini`·`Detail`에서는 숨긴다. 항목 비활성화는 자식 입력에도 적용하고, 항목이 숨겨지거나
다른 유형으로 바뀌는 동안 자식 상태는 보존한다.

```text
import vm ProjectModel
Window {
    ListItem(type: .DetailActions, label: vm.title,
             footerComponent: PushButton(text: "Publish", onClick: vm.publish))
}
```

C++에서는 `NativeListItemCompositeViewsValue<Builder, Factory, Image>::footer`에 `ii::gui::WidgetFactory`를
전달한다. 자식 생성 실패는 새 항목 생성을 거절하며 기존 콘텐츠 세대를 보존한다.
QML 객체의 자동 변환은 아직 독립 구현이 필요하다. 실제 플랫폼
렌더링, 접근성 기술 및 기기 검증도 별도로 필요하다.

### 사용자 뷰의 소유 항목

네 사용자 뷰 `leadingComponent`, `trailingComponent`, `previewComponent`,
`footerComponent`에서는 소유 항목을 `listItem`으로 읽을 수 있다. 항목마다 별도
스코프를 가지며 다른 `ListItem`을 중첩하면 그 항목의 뷰 안에서만 안쪽 소유자가
우선한다. 전달된 `View`, 조건부·반복 뷰에도 범위가 유지된다. `listItem`은 이
슬롯에만 정의되므로 다른 뷰에서는 앱의 모델을 사용한다.

```text
ListItem(type: .Toggle, label: "Selection", checked: vm.selected,
    footerComponent: PushButton(text: listItem.label,
        onClick: callback(listItem.editValue("checked", true))),
    onEdited: vm.edited)
```

직접 읽을 수 있는 상태는 `label`, `description`, `type`, `enabled`, `visible`,
`checked`, `quantity`, `selectorIndex`, `unitIndex`, `segmentIndex`, `inputText1`,
`inputText2`다. `options`는 소유 항목에 마지막으로 설정한 입력이고 `values`와
직접 노출한 편집값은 현재 유효 상태다. 따라서 모델이 편집 콜백을 아직 반영하지
않은 경우 두 값이 잠시 다를 수 있다. 값이나 설정이 바뀌면 이를 읽는 뷰를 갱신한다.

이벤트에 `callback(listItem.editValue(field, value))`로 연결한 함수는 체크, 수량, 세 인덱스와 두
문자열을 변경하고 기존 값별 콜백 및 `onEdited`를 발생시킨다. 수량은 유한수만
받아 범위로 제한하고, 인덱스는 범위 안의 정수만 받는다. 문자열은 유효한 제한
길이의 텍스트여야 한다. `listItem.stepQuantity(direction)`은 설정한 증분과
방향·범위를 따르며 `listItem.triggerAction(action)`은 설정된 명령과 액션 콜백을
호출한다. `primary`·`secondary` 이외의 액션은 더보기 설정을 사용하며 `more`는
`menu`의 별칭이다. 메뉴 액션
호출 자체가 기본 메뉴를 열지는 않는다. 항목 또는 해당 액션이 비활성화되었거나
값이 바뀌지 않으면 명령은 변경하지 않는다. 지원하지 않는 편집 필드는 무시한다.
기본 컨트롤이 사용자 뷰로 대체되더라도 명령은 소유 항목에 전달된다.

`listItem.triggerAction("primary", vm.payload)`처럼 두 번째 인자를 전달하면
`onActionEvent`의 `ListItemActionValues::custom_payload`에 불변 `std::any`가 담긴다.
같은 값 스냅샷의 다른 필드에는 실행 시점의 체크·수량·선택·입력값이 들어간다.
문자열은 `std::string`으로 보존하며 다른 복사 가능한 C++ 값은 원래 타입으로
보존한다. C++의 `ListItemContextValue<Image>::trigger_action`은 `std::shared_ptr<const std::any>`를
받는다. 기존 `onActionValuesTriggered`와 `ListFooterCommand::invoke()`의 인자 계약은
바뀌지 않으므로 사용자 페이로드가 필요한 소비자는 `onActionEvent`를 사용한다.

C++에서는 기존 무인자 팩토리와 별도로 `NativeListItemCompositeViewsValue<Builder, Factory, Image>`의
`leading_context`, `trailing_context`, `preview_context`, `footer_context`에
`ListItemContextViewFactoryValue<Builder, Image>`를 지정하거나, 생성한 항목의 `context()`를 요청할 수
있다. 같은 슬롯에 두 종류의 팩토리를 함께 지정하면 생성이 실패한다. 네 팩토리는
같은 소유자의 읽기 전용 `ListItemContextValue<Image>`를 받으며, `get()`·`observe()`와
`edit_value()`·`step_quantity()`·`trigger_action()`을 사용한다. 생성 중에는 값을
읽을 수 있지만 새 항목이 게시되기 전까지 편집할 수 없다. 항목 제거 후나 다른
스레드의 명령은 오류를 돌려주며 오래된 명령은 새 항목을 변경할 수 없다. 관찰자나
콜백이 항목을 없애거나 다시 편집하면 이전 세대의 후속 콜백은 생략한다. 콜백이
실패한 경우에는 이미 반영된 값을 유지하고 오류를 기록한다.

이 경계는 원본 QML 객체의 임의 속성 쓰기, `optionText`나 사용자 메서드 목록
전체를 제공하지 않는다. 앱은 필요한 값을 모델이나 명시적
콜백으로 전달해야 하며 원본의 모든 속성·메서드와 동등하다고 가정하지 않는다.

`LVUI.OwnedListItem.Model`, `Host`, `CompositeHost`, `ActionsHost`, `ActionsCodegen`, `ActionsGenerated`, `DetailActionsModel`, `DetailActionsHost`, `DetailActionsGenerated`, `DetailControlsModel`, `DetailControlsHost`, `DetailControlsCodegen`, `DetailControlsGenerated`, `TaskModel`, `TaskHost`, `TaskCodegen`, `TaskGenerated`, `PreviewRowModel`, `PreviewHost`, `PreviewCodegen`, `PreviewGenerated`, `FormModel`, `FormHost`, `FormCodegen`, `FormGenerated`, `CustomFooterHost`, `CustomFooterGenerated`, `CustomPreviewHost`, `CustomPreviewGenerated`, `CustomLeadingHost`, `CustomLeadingGenerated`, `CustomTrailingHost`, `CustomTrailingGenerated`, `ContextHost`, `ContextGenerated`, `MenuModel`, `MenuHost`, `MenuGenerated`, `SegmentsModel`, `SegmentsHost`, `StepperHost`, `StepperCodegen`, `StepperGenerated`, `SelectorHost`, `SelectHost`, `SelectCodegen`, `SelectGenerated`, `InlineEditHost`, `InlineEditCodegen`, `InlineEditGenerated`, `CompositeCodegen`, `CompositeGenerated`, `Codegen`, `Generated`는 배치 경계, 상태,
유효성, 독립 생성 코드, 갱신과 수명을 창을 열지 않는 위젯 트리에서 확인한다.

기존 C++의 `lvui::ListItem` 계산 모델은 제거되었다. 독립 소비자는
`ii::gui::ListItemModel`과 프레임워크 값 타입을 사용하거나 기존 LVUI 입력 값으로
조합할 때 `lvui::native::ListItemModel` 정책을 사용한다. 복합 행의 검증과 기하도
같은 iiGUI 모델을 사용한다. macOS arm64에서 기본·상세·값·복합 행의 모델과
생성·호스트 및 설치 소비자 검사 22건을 통과했다. 실제 창·GPU와 다른 운영체제는
이 검사에 포함되지 않는다.

기본 행의 디자인 값과 그리기 정책 바인딩은 iiGUI를 직접 참조하지 않는다. 앱이
행 모델과 선형 색 변환기를 공급해 iiGUI 그리기 및 호스트를 조합한다. macOS arm64
설치 헤더와 기본·상세·폼·가상 목록 경로를 헤드리스로 검사했다. 행 호스트 바인딩도 iiGUI를 직접 참조하지 않으며 앱이 이미지와 그리기 포트를
공급한다. 복합 행의 그리기·입력·하위 위젯 조합과 상세·작업·미리보기·폼 배치 및 수량 상태는 iiGUI가 담당한다. LVUI는 입력값·문맥·토큰과 교체 가능한 바인딩을 제공한다.

목록 세그먼트의 선택·이동·기하, 버튼 조합과 위젯 수명은 iiGUI의 `ListItemSegmentsModel.h`와 `NativeListItemSegmentsView.h`가 소유한다. LVUI는 디자인 값과 Core 외 SDK를 참조하지 않는 `ListItemSegmentsBinding`을 제공한다. 설정은 게시 전에도 준비할 수 있으며 퇴역 시 콜백과 명령을 해제하고 남은 명령을 전달하지 않는다. macOS arm64에서 독립 소비자와 복합 목록·생성 경로를 창 없이 검사했다.

`iiGUI/NativeListItemCompositeView.h`의 구성 요소 묶음은 앱 또는 AOT 생성 코드가 주입한다. 상세·작업·미리보기·폼 배치 및 수량 계산은 각각 iiGUI의 `ListItem*Layout.h`에 있다. `ListItemComposite` 선언은 복합 구현을 명시적으로 선택하며 `ListItem(type: ...)`과 같은 입력·이벤트 계약을 사용한다. 기존 배치·상태 테스트는 내용을 유지하고 iiGUI 구현에 연결했다. 독립 설치 소비자와 여러 생성 화면의 동시 포함도 창 없이 검사한다.

복합 항목 메뉴의 깊이·항목 수·텍스트 검증은 `iiGUI/ListItemMenuModel.h`에 있다. LVUI는 메뉴 값과 `ListItemMenuBinding`을 제공한다.
