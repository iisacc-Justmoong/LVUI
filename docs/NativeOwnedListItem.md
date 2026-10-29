# LVUI 독립 목록 항목

`ListItem`은 LVUI가 직접 소유한 행의 데이터, 배치, 색상과 입력 표면이다. Qt, QML,
LVRS 위젯에 의존하지 않으며 일반 [목록 화면](NativeOwnedListView.md)과 같은 배치·그리기
코드를 사용한다. 앱은 `ListItemInput`과 `NativeListItem::create()`를 사용해 개별 행을
만들거나 다음과 같이 기본 선언으로 생성할 수 있다.

```text
import vm RowModel
Window {
    ListItem(type: .Navigation, label: vm.label, description: vm.description,
             selected: vm.selected, onClick: vm.activate)
}
```

현재 독립 선언은 `.Mini`, `.Detail`, `.Navigation`, `.Toggle`, `.Checkable`, `.Action`, `.ActionGroup`, `.Stepper`, `.Select`와 같은 소문자 표기를 지원한다.
기본 폭과 높이는 각각 170×20, 194×44, 탐색·토글·체크의 280×44와
액션·스테퍼·선택 유형의 400×44이며 명시적인 `width`와 `height`로
바꿀 수 있다. 이름이나 직접 제공한 래스터, 교체 가능한 아이콘 공급자로 아이콘을
선택하고, 선택·비활성·호버·누름·포커스 색상과 간격을 조정할 수 있다. 아이콘은
기본적으로 비어 있다. 높이가 32 이상인 경우 설명을 별도 줄에 표시한다.

`selected`는 앱이 관리하며 클릭은 `onClick`을 전달한다. `enabled`, `interactive`,
`selectable`, `visible`이 입력을 제어한다. 갱신 가능한 속성은 반응형으로 연결된다.
잘못된 설정이나 아이콘은 거부하고 마지막 정상 설정을 유지한다. 렌더링과 접근성에
사용하는 텍스트는 UTF-8 경계에 맞춰 각각 최대 8,192바이트로 제한하며 원본 설정은
유지한다. 앱이 소유한 상태는 컴포넌트가 되돌리지 않는다.

C++의 `NativeListItemComposite`는 LVUI의 `ToggleSwitch`와 `CheckBox`를 조합해 토글과
체크 유형을 제공한다. 토글은 기본적으로 켜져 있고 체크는 기본적으로 꺼져 있으며,
명시적인 `checked`로 초기값을 정할 수 있다. 값이 바뀌면 `checked_changed`와
`edited("checked", value)`를 순서대로 전달한다. 앱이 현재 값을 반응형으로 다시
제공하는 경우 이후 이벤트를 유지한다. 각 컨트롤의 활성 여부와 표시 여부는 별도로
설정할 수 있다. 선언에서는 `checked`, `toggleEnabled`, `selectionEnabled`, `showToggle`, `showSelection`과
`onCheckedChanged`·`onEdited`를 사용한다. `onEdited`의 값은 `lvui::ListItemValue`다.

C++의 `.action`과 `.action_group` 유형은 LVUI `ListFooter` 슬롯으로 기본·보조·추가
액션을 조합한다. `ListFooterSlot`에 표시, 활성 여부, 아이콘 및 교체 가능한 명령을
전달하며 `action_triggered`는 `primary`, `secondary`, `more`와 현재 슬롯을 전달한다.
명령 실행 중 항목이 제거되면 오래된 이벤트는 전달하지 않는다. 선언에서 `primaryAction`, `secondaryAction`, `moreMenu`는 문자열이나
`ListFooterSlot`을 받으며 `onActionTriggered`로 결과를 전달한다.

`.stepper` 유형은 LVUI `NativeStepper`와 [수량 모델](ListItemQuantity.md)을 조합한다.
기본 수량은 1, 최소값은 0, 최대값은 양의 무한대, 증분은 1이다. 수량과 범위,
증분을 변경할 수 있고 비활성·숨김 상태나 범위 끝에서는 값 변경 이벤트가 발생하지 않는다.
변경 시 `quantity_changed`와 `edited("quantity", value)`를 차례로 전달한다. 선언에는
`quantity`, `minimumQuantity`, `maximumQuantity`, `stepSize`, `quantityWidth`, `quantitySpacing`,
`stepperEnabled`, `showStepper`, `onQuantityChanged`, `onEdited`를 사용한다.
`showStepper: false`는 조절 버튼만 감추고 현재 수량은 계속 표시한다.
범위만 바뀌면 현재 수량을 새 범위로 제한하며, 명시 수량이 범위 밖이거나 잘못된
설정은 이전 상태를 유지한다. 앱이 현재 수량을 반응형으로 되돌려 제공해도 편집
이벤트는 전달된다.

`.select` 유형은 LVUI의 [선택기](NativeOwnedListItemSelector.md)를 사용한다. `selector`에는
문자열 목록 또는 C++의 `ListItemSelectorConfiguration`·`ListItemOption` 목록을 제공한다.
`selectorIndex`, `selectorWidth`, `selectorEnabled`, `showSelector`와
`onSelectorIndexChanged`로 상태를 제어한다. 변경 시 `selector_index_changed` 다음에
`edited("selectorIndex", value)`를 전달하며 편집 값은 `std::int64_t`다. 빈 목록은
기본 문구를 표시하고, 비활성 항목은 선택하지 않는다.

인라인 편집 및 리소스·미디어·작업·폼 유형과 사용자 지정 자식
콘텐츠는 아직 독립 구현이 필요하다. 지원하지 않는 선언은 명시적으로 오류를 반환한다. 실제 플랫폼 렌더링,
접근성 기술 및 기기 검증도 별도로 필요하다.

`LVUI.OwnedListItem.Model`, `Host`, `CompositeHost`, `ActionsHost`, `ActionsCodegen`, `ActionsGenerated`, `StepperHost`, `StepperCodegen`, `StepperGenerated`, `SelectorHost`, `SelectHost`, `SelectCodegen`, `SelectGenerated`, `CompositeCodegen`, `CompositeGenerated`, `Codegen`, `Generated`는 배치 경계, 상태,
유효성, 독립 생성 코드, 갱신과 수명을 창을 열지 않는 위젯 트리에서 확인한다.
