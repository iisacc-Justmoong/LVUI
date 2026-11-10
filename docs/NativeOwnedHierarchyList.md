# LVUI 독립 계층 목록

iiGUI의 `BasicNativeHierarchyList`는 앱이 제공한 계층 투영과 행 계약으로 가상 행,
입력·선택·드래그와 위젯 수명을 관리한다. LVUI는 값·바인딩과 관찰 가능한 행
컨텍스트를 제공하며 생성된 앱이 두 모듈을 조합한다. Qt, QML, LVRS 위젯을 사용하지 않는다. 읽기와 쓰기는
[독립 계층 모델](OwnedHierarchyModel.md)의 교체 가능한 인터페이스로 제공하며,
목록의 선택과 펼침은 화면 상태이고 문서 편집이 아니다.

```text
import vm LayerBackend
Window {
    HierarchyList(controller: vm.controller, modelRevision: vm.revision,
                  width: 240, height: 400, rowHeight: 20,
                  onActiveChanged: vm.activate,
                  onExpansionChanged: vm.setExpanded)
}
```

`controller`는 앱이 소유한 `std::shared_ptr<lvui::HierarchyController>`이다.
생략하면 빈 `HierarchyMemory`와 컨트롤러를 이 목록이 소유한다. 앱의 데이터가
바뀌면 관측 가능한 정수 `modelRevision`을 변경해 목록을 갱신한다. 읽기 소스는
내용이 변경될 때마다 증가하는 고유한 스냅샷 리비전을 발행해야 한다. 사용자
입력 직전에 소스가 변경된 경우 이전 행의 이벤트는 버리고 새 투영을 만든다.
`onActiveChanged`는 키, 선택적 ID(없으면 -1), 소스 인덱스를 전달하고,
`onExpansionChanged`는 키와 펼침 여부를 전달한다.

기본 또는 키별 높이를 가진 행의 보이는 범위와 `overscan`만 마운트한다. 스크롤 중 계속 보이는 키는
행 객체와 입력 상태를 재사용하고, 범위를 벗어난 행은 해제한다. 소스 삽입과 제거
또는 행 높이 변경 후에는 가능한 한 이전의 보이는 키와 행 내부 위치를 유지한다. 소스나 설정의
잘못된 후보, 존재하지 않는 아이콘은 이전에 표시된 내용을 유지하고 오류를 돌려준다.
복구된 값을 전달하면 다시 갱신된다. 스냅샷이 게시되기 전에 외부 앱이 변경한 상태나
이미 전달한 콜백을 되돌리지는 않는다.
상위 복합 뷰가 게시되는 동안 행 콘텐츠 게시가 일시적으로 막히면, 상위 게시가
완료된 뒤 다음 레이아웃에서 보이는 행을 다시 구성한다.

선언형 입력은 `controller`, `modelRevision`, `viewRevision`, 크기, `rowHeight` 또는
`generatedRowHeight`, `overscan`, `enabled`, `showIndicator`,
`keyboardNavigationEnabled`, 아이콘 공급자와
`iconRevision`, `countView`, `countRow`, `itemDelegate`, `itemRow`, 들여쓰기·패딩·아이콘·접기 표시·개수 크기 및 행 색상을 받는다.
`generatedIndentStep`/`indentStep`, `generatedIconSize`/`iconSize`,
`generatedChevronSize`/`chevronSize`는 각각 별칭이다. 값은 반응형으로 갱신된다.
아이콘 공급자를 바꿀 때는 `iconRevision`도 갱신한다. `editable: true`일 때는
같은 계층 소스를 편집하는 `std::shared_ptr<lvui::HierarchyEditor>`를 `editor`로
제공한다. 컨트롤러와 위젯은 생성한 스레드에서 사용한다.

`rowHeight`는 높이를 따로 지정하지 않은 행의 기본값이다. 앱은
`controller->set_row_extent(key, height)`로 개별 행의 높이를 1–8192로 설정하고
`std::nullopt`로 기본값을 복원할 수 있다. 변경은 문서가 아닌 화면 상태다. 앱이
컨트롤러를 직접 변경한 뒤 선언형 목록을 갱신하려면 별도의 관찰 가능한 정수
`viewRevision`을 변경한다. C++에서 직접 조합한 목록은 `set_options()`를 다시
호출한다. `modelRevision`은 소스 데이터 변경에만 사용한다.
매우 작은 행이 큰 화면에 한꺼번에 많이 보이면 iiGUI의 노드 및 키별 콘텐츠
예산에 도달할 수 있다. 예산을 넘는 후보는 오류로 거절하며 현재 유효한 화면을 보존한다.

`countView`는 각 마운트된 행의 기본 숫자 대신 표시할 단일 View이다. 같은 선언을
행마다 별도 인스턴스로 만들고 원래 스코프의 반응형 값과 콜백을 유지한다. 해당 행의
개수가 0 이상이고 `countWidth`가 양수일 때만 표시하며, 행 비활성화와 클리핑을
따른다. 예를 들어 `countView: Badge(text: vm.badge)`는 각 보이는 행에 `Badge`를
만들고 `vm.badge` 변경을 모든 마운트된 배지에 전달한다. 행마다 다른 값을 표시하려면
`countRow`에 유효한 식별자를 문자열로 지정하고 그 이름으로 행을 읽는다.
이 이름은 `countView` 내부에서만 유효하며 같은 이름의 외부 변수를 그 범위에서만
가린다. `countRow`는 `countView`와 함께 사용해야 한다. `[View]`나 숫자 같은
개수 View와 잘못된 행 이름은 거부한다.

```text
HierarchyList(controller: vm.controller, modelRevision: vm.revision,
              countRow: "item", countView: Label(text: item.entry.count))
```

읽기 전용 행은 `entry` 외에도 `source_index`, `depth`, `child_count`, `expanded`,
`selected`, `enabled`, 실제 행의 `height`를 제공한다. 유지되는 키의 값이나 선택이 바뀌면 해당 행의
반응형 바인딩을 갱신한다. 행 이름을 지정하지 않으면 외부 스코프만 사용한다.

`itemDelegate`는 기본 아이콘·라벨·개수를 대신할 단일 사용자 View이다.
`itemRow: "item"`을 함께 지정하면 그 View 안에서 `item.entry.label`,
`item.selected` 등 위와 같은 읽기 전용 행 데이터를 관찰한다. 행 이름을 지정하지
않으면 기존 외부 스코프의 값만 사용한다. 각 키의 View와 컨텍스트는 계속 보이는
동안 유지하고 가상화 범위를 벗어나면 함께 해제한다.

```text
HierarchyList(controller: vm.controller, modelRevision: vm.revision,
              itemRow: "item", itemDelegate: Label(text: item.entry.label))
```

선택 배경·포커스·접기 표시와 기본 접근성 이름은 목록의 행이 계속 제공한다.
사용자 View는 행 크기로 잘리며 개수가 음수여도 표시된다. 기본 아이콘을 그리지
않으므로 사용하지 않는 기본 아이콘 이름은 해석하지 않는다. View 안의 버튼이나
입력 컨트롤은 자신의 포인터 입력을 우선하며, 표시 전용 영역은 행 클릭과 드래그에
사용된다. `itemDelegate`와 `countView`는 함께 사용할 수 없다. `itemRow`는
`itemDelegate`와 함께 유효한 식별자로 지정해야 한다.

C++ 앱은 조합된 iiGUI 호스트의 `create`에 `HierarchyCountViewFactoryValue<Builder>`로
개수 콘텐츠와 `shared_ptr<const HierarchyRowContext>`를 받는다. 각 컨텍스트의
`get()`은 원본 항목·인덱스·깊이·자식 수·펼침·선택·활성 상태를 읽고,
`revision()`, `observe()`와 `read_dependency()`는 iiSDKCore 관찰과 LVUI
`ReadContext::read(*row)`에 연결한다. 키가 유지되는 동안 같은 컨텍스트와 뷰를
유지하고, 게시된 데이터나 선택이 바뀌면 관찰자에게 알린다. 범위를 벗어나거나
목록이 사라지면 컨텍스트를 닫으며 이후 조회와 다른 스레드의 조회는 거부한다.
`get()`의 참조는 다음 게시 또는 컨텍스트 종료 이후에 보관하지 않으며, 오래 써야 하는
값은 복사한다. 팩터리는 목록 생성 시 정하므로 바꾸려면 새 목록을 구성한다.
실패한 새 뷰 후보는 기존 행을 보존하고 후보의 컨텍스트를 닫는다. 팩터리나 관찰자가
외부에 이미 수행한 부수효과는 되돌리지 않는다.

같은 행 컨텍스트를 받는 `HierarchyRowViewFactoryValue<Builder>`는 조합된 iiGUI
호스트의 `create` 마지막 인자로 전달한다. 개수 팩터리와 동시에 지정하지 않으며, 개수 팩터리가 없는
자리는 `{}`로 둔다. 사용자 행의 배치와 입력은 위 선언형 계약과 동일하다.

현재 행을 클릭하고 접기 표시 또는 행의 좌우 키로 펼칠 수 있다. 위·아래, Home,
End, Page Up과 Page Down 키는 선택할 수 없는 항목을 건너뛰고 대상 행을 표시한 후
포커스를 옮긴다. 오른쪽 키는 펼친 행에서 자식으로, 왼쪽 키는 접힌 행에서 부모로
이동한다. 키보드 탐색은 끌 수 있다. 편집 가능한 행은 포인터로 끌 수 있으며
원본·대상 행을 미리 표시하고 놓는 순간에만 리비전이 포함된 이동을 요청한다.
콜백 `onItemMoved`는 성공한 `lvui::HierarchyMovePlan`을 받는다. 편집이 성공한
뒤 새 화면을 만들지 못하더라도 성공한 이동은 콜백으로 전달하고 화면 오류를
호출자에게 알린다. 일반 클릭과 펼침 표시는 유지하고, 소스 변경·취소·편집 비활성화 시 진행 중인 후보를 버린다.
가장자리 스크롤은 포인터 이동 이벤트당 포인터가 가리키는 행의 높이만큼 진행한다.
페이지 키는 실제 높이에서 화면 한 장의 거리를 이동하고 한 행이 화면보다 크더라도
다른 행으로 이동할 수 있다. 운영체제 보조 기술 연결의 실제 검증은 아직 남아 있다.
미지원 선언은 컴파일 오류로 알려준다.

`LVUI.OwnedHierarchyList.Host`는 1,000개 항목의 가상화, 키별 재사용과 해제,
소스 비변경, 삽입 후 기준점, 오래된 이벤트, 잘못된 아이콘 복구 및 콜백 중 제거를
헤드리스 트리에서 검사한다. `LVUI.OwnedHierarchyList.Keyboard`은 방향·페이지 이동,
스크롤과 포커스, 부모·자식 이동, 비활성 항목 및 오래된 키 이벤트를 검사한다.
`LVUI.OwnedHierarchyList.Drag`는 클릭과 드래그 구분, 하위 트리 이동, 오래된
데이터 취소, 편집 거절, 콜백 중 제거와 접기 표시를 검사한다. 생성 코드 검사와
헤드리스 생성 위젯 검사도 제공한다.
`LVUI.OwnedHierarchyList.CountView`는 행별 관찰과 재정렬, 선택, 숨김·비활성화,
재진입, 실패 복구, 스레드와 퇴역을 검사한다. `CountViewGenerated`는 실제 생성된
선언형 뷰의 행별 인스턴스, 반응형 입력, 표시와 가상화 수명을 검사한다.
`CountRowGenerated`는 생성된 뷰가 행별 값과 선택·순서 변경을 관찰하고 재사용 및
가상화 해제 뒤에도 올바르게 구성되는지 검사한다.
`RowDelegate`와 `RowDelegateGenerated`는 사용자 행의 관찰·갱신·퇴역과 실패 복구를,
`RowDelegateDrag`와 `RowDelegateKeyboard`는 내부 버튼·클릭·드래그와 키보드 탐색을 검사한다.
이들 검사는 가변 행의 크기 전달과 유지·기준점·클릭·드래그·페이지 이동 및 문서
비변경도 다룬다.
이는 실제 창, 플랫폼 접근성 또는 기기 렌더링 검증을 의미하지 않는다.

C++에서는 `HierarchyListBinding`에 iiGUI 목록·항목 구현과 이미지 디코더를 주입한다.
LVUI 바인딩은 iiSDKCore 외 SDK를 직접 참조하지 않는다. 퇴역하면 진행 중인 행 게시와
콜백이 끝난 뒤 원천·아이콘 캐시·사용자 팩터리·콜백을 해제한다. 행을 만드는 도중 새
원천이 도착하면 마운트된 키를 먼저 보존하고 보류된 최신 투영을 이어서 적용한다.
macOS arm64의 독립 설치 소비자와 LVUI 생성·헤드리스 검사를 통과했다.
