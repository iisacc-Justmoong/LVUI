# LVUI 독립 표 호스트

`iiGUI/NativeTableView.h`의 `BasicNativeTable`은 [독립 표 소스와 선택 컨트롤러](OwnedTableModel.md)를 스크롤·입력·그리기에 연결하고 표의 격자·선택 영역·접근성을 처리한다. LVUI는 데이터, 사용자 정의 셀 계약과 `TableBinding`을 제공한다. 앱 또는 AOT 생성 코드가 입력 필드·메뉴·헤더·사용자 정의 셀 호스트를 주입해 조합한다. C++ 앱은 `TableInput::controller`에 `std::shared_ptr<TableController>`를 넣고 조합한 호스트의 `create()`를 호출한다. Qt, QML과 LVRS 위젯은 사용하지 않는다.

```text
import vm TableBackend
Window {
    Table(model: vm.controller, editor: vm.editor, editable: true,
          modelRevision: vm.revision, width: 660, height: 300,
          onSelectionChanged: vm.select, onCellInputEdited: vm.edited,
          onCellInputSubmitted: vm.submitted, onCellInputRejected: vm.rejected)
}
```

`model`과 `controller`는 별칭이며 생략하면 이 표가 빈 메모리 소스와 컨트롤러를
소유한다. 크기, 헤더 높이, 셀 여백, 열 최소·기본 너비, 텍스트와 색상, 선택·키보드
사용 여부, 유연 열 맞춤과 표시 이름을 설정할 수 있다. `onSelectionChanged`는
`std::optional<lvui::TableSelection>`을 전달한다. 선언형 입력은 반응형이다.

## 사용자 정의 열 View

`columnViews`는 고정된 열 키마다 `header`와 `cell` View를 선택한다. 생략한 위치는
기본 콘텐츠를 사용한다. `headerContext`와 `cellContext`는 각 View에서 사용할
읽기 전용 문맥 이름이다. 키 목록과 View 구성은 컴파일 시 정해지며 데이터는 반응형이다.

```text
import vm TableBackend
const Name(value: String): View = HStack { Label(text: value) }
Table(model: vm.controller, modelRevision: vm.revision,
      headerContext: "heading", cellContext: "cell",
      columnViews: [
          TableColumn(key: "name", header: Label(text: heading.text), cell: Name(value: cell.text)),
          TableColumn(key: "size", cell: Label(text: cell.text))
      ])
```

모든 열의 기본 대리자로 `headerDelegate` 또는 `headerCellDelegate`,
`cellDelegate` 또는 `delegate`를 지정할 수도 있다. 키별 View가 해당 대리자보다
우선한다. `columnViews`는 `TableColumn` View 선언의 목록과 고유한 문자열 키를 요구하며 중복 키,
내용 없는 항목과 잘못된 문맥 이름은 컴파일 오류다. C++에서는 `NativeTableViewsValue<Builder>`의
`header`·`cell`과 키별 `headers`·`columns` 팩터리를 조합한 표 호스트의 `create()`에
전달한다.

문맥에는 `row_key`, `column_key`, `row_index`, `column_index`, `text`, `value_type`,
`x`, `y`, `width`, `height`, `row_span`, `column_span`, `header`, `enabled`,
`editable`, `selected`, `current`, `sorted`, `descending`이 있다. 인덱스는 0부터
시작하고 좌표는 각 머리글·본문 뷰포트 기준이다. 병합 셀은 기준 셀과 병합 크기를
제공한다. C++ 팩터리는 `TableViewContext::get()`과 `observe()`를 사용한다.
보이는 동안 같은 행·열 키는 View와 지역 상태를 보존하고 값·선택·정렬·위치 변경을
관찰한다. 화면 밖으로 나간 문맥은 닫히며 다시 들어오면 새 View가 생성된다.
표, 행, 열 또는 셀이 비활성화되면 해당 사용자 View의 입력도 비활성화된다.

사용자 View가 없는 셀과 비상호작용 View는 표의 선택·키보드·정렬 동작을 그대로
사용한다. View 내부의 버튼 등은 자신의 입력을 처리하므로 그 클릭에 대한 앱 동작은
ViewModel에서 연결해야 한다. 예를 들어 `cell` View 안에
`PushButton(text: cell.text, onClick: callback(vm.openCell(cell.row_key)))`를 두고
`vm.openCell`이 해당 키를 캡처하는 콜백을 반환하도록 할 수 있다. 자식 컨트롤이
포인터와 키보드 입력을 받는 동안 표가 같은 입력으로 선택·정렬하지 않는다.
사용자 View는 기본 텍스트·정렬 아이콘을 대신하며,
선택 배경과 격자 및 접근성 표 요소는 표가 소유한다. 기존 셀 편집기는 View 위에
표시된다. 사용자 View가 실패하면 직전의 표시 범위를 복원하며 앱 데이터 변경 자체는
되돌리지 않는다. 많은 복합 View는 별도의 iiGUI 위젯 한도에도 적용된다.

전체 표와 독립 머리글은 같은 열별 아이콘·여백 표시를 사용한다. C++에서는
`TableInput::header_presentation`의 `default_leading_icon`, `default_trailing_icon`,
`icon_size`, `icon_spacing`, `column_appearance`를 설정하고 `header_icon_source`에
교체 가능한 `IconSource`를 줄 수 있다. 선언형에서는 각각 `headerDefaultLeadingIcon`,
`headerDefaultTrailingIcon`, `headerIconSize`, `headerIconSpacing`,
`headerColumnAppearance`, `headerIconSource`, `headerIconRevision`으로 연결한다.
기본 선행 아이콘은 `generalchevronDown`, 크기는 18, 아이콘과 제목 사이 간격은 7이다.
열별 `TableHeaderColumnAppearance`는 열 키와 선택적인 여백·선행·후행 아이콘을
담는다. 빈 아이콘 이름은 해당 아이콘을 숨긴다. 여백 미지정 시 표의 `cellPadding`을
사용한다. 지정한 열 키가 현재 표에 없어도 스타일을 보관하므로 삭제된 열이 같은
키로 다시 삽입되면 설정을 다시 적용한다. 그 스타일에 명시된 아이콘도 미리 검증한다.

공급자를 지정하면 해당 공급자에서 아이콘을 찾고, 지정하지 않으면 LVUI의 독립
카탈로그를 사용한다. 가변 공급자는 자체 `revision()`과 반응형
`headerIconRevision`을 함께 갱신한다. 잘못된 이름이나 공급자 오류, 래스터 총량
64 MiB 초과는 기존 표를 교체하기 전에 거절한다. 정렬 표시, 가로 스크롤, 열 폭
조작과 개별 접근성 요소는 그대로 동작하며 표시 설정만 바꾸는 작업은 문서 리비전을
변경하지 않는다. 아이콘의 이름·크기·여백과 공급자 제약은
[독립 표 머리글](NativeOwnedTableHeader.md)을 따른다.

병합된 셀은 기준 셀의 내용과 범위로 표시하고 내부 격자선을 감춘다. 클릭·선택·
방향키·편집은 기준 셀과 전체 병합 크기를 따른다. 외부 편집 결과는
`modelRevision`으로 갱신한다. `mergeEditor`와 `onMergeChanged`의 계약,
화면의 그리기 예산과 붙여넣기 제한은 [표 병합](TableMerge.md)을 따른다.

편집은 기본적으로 꺼져 있다. 켜려면 소스와 같은 데이터에 적용하는 교체 가능한
`TableEditor`를 명시해야 한다. 선택한 편집 가능 셀에서 Enter로 입력을 시작하고
Enter로 확정하거나 Escape로 취소한다. 다른 셀을 누르거나 편집 중인 셀이 화면 밖으로
스크롤되면 작성 중인 값은 취소된다. 입력 중에는 원본이 바뀌지 않으며, 검증 거절이나
시작 이후의 외부 변경으로 확정에 실패하면 입력값을 유지한다. 앱은 오류를 표시하거나
수정 후 다시 시도할 수 있다. 선언형 편집 및 확정 콜백은 행 인덱스·열 인덱스·문자열을,
거절 콜백은 여기에 값 유형을 더해 전달한다. C++ 콜백에는 키와 인덱스, 문자열,
값 유형이 담긴 `TableCellEvent`가 전달되며 거절 시 `TableError`도 함께 제공된다.
`begin_edit()`, `commit_edit()`, `cancel_edit()`과 `editor()`로 앱이 편집을 제어할 수
있다. 자세한 확정·충돌 계약은 [셀 편집 세션](TableEditing.md)을 따른다.
팔레트만 바꾸는 갱신은 편집 중인 텍스트·한글 조합과 포커스를 유지하고 편집기의
배경·텍스트·포커스·선택색을 새 팔레트에 맞춘다. 열린 구조 메뉴와 크기 조절
미리보기도 유지한다. 데이터나 치수 등 다른 설정이 바뀌는 경우의 기존 취소·충돌
규칙은 그대로 적용한다.

선택 범위는 복사할 수 있고, 편집이 켜져 있으면 잘라내거나 붙여넣을 수 있다.
`clipboardEnabled`로 이 기능을 제어하며 기본값은 켜짐이다. `onRangeValuesChanged`는
실제로 변경된 범위의 `lvui::TableRange`를 전달한다. C++에서 클립보드 함수를 주입하거나
`copy_selection()`, `cut_selection()`, `paste_clipboard()`, `paste_text()`를 호출할 수
있다. 범위 크기와 거절 조건은 [표 범위 클립보드](TableClipboard.md)를 따른다.

`sortingEnabled`와 교체 가능한 `orderEditor`를 지정하면 헤더에서 행 정렬을
요청할 수 있다. 기본 정렬은 문자열의 바이트 순서를 사용하며, 앱은 `sorter`로
숫자·날짜 등을 처리할 수 있다. `onRowsSorted`는 열 인덱스와 방향을 전달한다.
자세한 리비전과 정렬 정책은 [표 행 정렬](TableSorting.md)을 따른다.

`structureEditor`는 행·열 삽입, 삭제와 크기 변경의 독립 경계다. 선택적으로
`columnResizeEnabled`와 `rowResizeEnabled`를 켜 경계를 끌어 크기를 조절할 수 있다.
`structureControlsVisible`로 행·열 추가 조작을 표시하고, 구조 편집기가 있는 셀에는
삭제 메뉴를 사용할 수 있다. 앱은 `structureFactory`로 새 행·열의 값과 키를 교체한다.
행이나 열이 없는 경우 첫 행·열 추가 표면을 표시한다. 보이는 기존 행·열과 첫
행·열의 추가 표면은 각각 버튼 역할과 접근성 이름을 가져 키보드와 접근성 활성화를
지원한다. 스크롤한 뒤에도 보이는 항목의 키와 위치를 따른다.
동작과 충돌 처리는 [표 구조와 크기 편집](TableStructure.md)을 따른다.

기본 크기는 660×300, 헤더와 행의 기본 높이는 24, 셀 좌우 여백은 8이다. 열의
명시적 너비는 최소 32이며 너비가 0인 열은 남은 폭을 나누거나 설정된 기본 너비를
사용한다. 행 높이는 소스가 지정한다. 헤더는 세로로 고정되고 가로 스크롤은 본문과
공유한다. 화면 밖 행과 열의 셀을 그리지 않으며, 내용 삽입 뒤에는 가능할 때 각 축의
보이는 키와 내부 위치를 유지한다. 사용자가 움직인 축에는 이전 기준점을 적용하지 않는다.

포인터를 누르거나 끌어 사각 범위를 선택하고, 방향키·Home·End와 Shift로 셀을
이동하거나 범위를 확장할 수 있다. 비활성 셀은 선택하지 않는다. `selection_changed`는
키 기반 선택을 전달한다. 화면 선택과 스크롤은 문서 리비전을 변경하지 않는다.
표 전체와 화면에 보이는 셀은 iiGUI의 접근성 요소로도 제공한다. 셀은 열 제목과
행 번호, 문자열 값, 선택·활성 상태를 전달한다. 병합 셀은 기준 셀이 화면 밖에 있어도
보이는 범위를 하나의 기준 셀로 노출한다. 초점 요청은 선택만 변경하고 활성화는 선택한
셀의 편집이 가능하면 기존 편집기를 연다. 이 경로도 비활성 셀과 오래된 소스 이벤트를
거절하며 선택 자체는 문서 리비전을 바꾸지 않는다. 스크롤 중 계속 보이는 같은 행·열 키는
식별자를 유지하고 화면에서 사라진 셀은 퇴역한다. 다국어 값의 접근성 발췌는 UTF-8
문자 경계를 보존하며 제목은 최대 256바이트, 값은 최대 1,024바이트다.
화면에 보이는 각 열 머리글은 같은 표에 속하는 별도 요소다. 활성화 가능한 열은
접근성 동작으로 기존 오름차순·내림차순 정렬을 실행하고 현재 방향을 전달한다.
현재 방향은 지역화된 문자열 값으로 전달하며 운영체제의 구조화된 정렬 방향 속성은
아직 제공하지 않는다.
정렬이 꺼졌거나 열이 비활성화된 경우에는 정렬하지 않는다. 스크롤 중 계속 보이는
열의 식별자는 유지하고 화면을 벗어난 열은 퇴역한다. 정렬 직전 소스가 바뀌면
오래된 요청은 거절하고 현재 데이터를 다시 표시한다.

앱은 `TableInput::text`에 `lvui::TableText` 값을 제공하거나 선언형
`Table(accessibilityText: vm.tableText)`에서 C++ 백엔드의 같은 값을 전달해 표의
이름을 지역화할 수 있다. 관찰 가능한 값을 바꾸면 화면의 셀, 추가 버튼, 열린 셀
편집기의 접근성 이름과 구조 메뉴의 이름·삭제 항목이 갱신된다. 표 전체의 이름은
기존 `appearance.accessibility_name` 또는 `accessibilityName`으로 지정한다. 예를 들면:

```cpp
lvui::TableText text;
text.unnamed_column = "{column}열";
text.cell_name = "{row}행의 {column}";
text.merged_description = "{columns}열과 {rows}행 병합";
text.add_row_after = "{row}행 다음에 삽입";
text.add_column_after = "{column}열 다음에 삽입";
text.add_first_row = "첫 행 추가";
text.add_first_column = "첫 열 추가";
text.edit_cell = "셀 편집";
text.structure_menu = "표 구조";
text.delete_row = "행 삭제";
text.delete_column = "열 삭제";
text.header_group = "열 머리글";
text.sortable_column = "정렬 가능";
text.sorted_ascending = "오름차순";
text.sorted_descending = "내림차순";
// C++ 사용: input.text = text; table.set_options(input);
// 선언형 사용: Observable<lvui::TableText> 백엔드 값을 accessibilityText로 연결한다.
```

문장별 필수 자리표시는 `unnamed_column`의 `{column}`, `cell_name`의 `{column}`·
`{row}`, `merged_description`의 `{rows}`·`{columns}`, 추가 문장의 `{row}` 또는
`{column}`이다. 각각 한 번씩 순서와 관계없이 사용하며 `{{`·`}}`로 중괄호를 직접
쓸 수 있다. 행·열 번호는 1부터 시작하는 십진수다. 열 제목에 포함된 중괄호는
재해석하지 않는다. 각 문자열은 비어 있지 않은 유효한 UTF-8이어야 하고 최대
512바이트다. 잘못된 자리표시나 문자열은 기존 표를 바꾸기 전에 거절한다. 기본값은
영어이며 수량별 복수형 선택은 자동 처리하지 않는다. 필요하면 앱이 언어에 맞는
중립적 문장을 제공해야 한다. 숫자의 지역별 표기는 아직 지원하지 않는다. 언어 변경은 문서 데이터나
선택을 바꾸지 않으며, 계속 보이는 셀의 식별자와 입력 중인 조합을 보존한다.
앱이 소스를 교체하면 `source_revision`을 변경해 갱신한다. 이벤트 직전에 소스가
바뀌었다면 오래된 이벤트를 전달하지 않고 새 화면을 만든다. 설정이나 새 화면의
검증 실패는 마지막으로 유효한 위젯을 보존한다. 위젯과 컨트롤러는 생성한 스레드에서
사용한다.

한 화면에서 그리는 셀은 호스트의 그리기 예산에 맞춰 최대 3,072개이며 이를 초과하는
후보는 오류로 거절한다. 병합이 보이는 화면은 더 작은 1,000셀 예산을 사용한다.
긴 셀 텍스트는 화면 그리기 예산 안에서 UTF-8 경계를 지켜
앞부분을 표시하고 원본 데이터는 보존한다. 운영체제 보조 기술에는 표 전체의 이름과
보이는 행·열 추가 버튼, 열 머리글과 가상 셀을 제공한다. 별도의 행·열 객체와 OS별 스크린 리더,
실제 플랫폼 렌더링과 입력 검증은 아직 남아 있다. 미지원 선언은
컴파일 오류로 알려준다.

`LVUI.OwnedTable.Host`는 1,000행의 가상 범위와 두 축 스크롤, 그리기, 포인터·키보드
선택, 오래된 이벤트, 비활성 셀, 긴 UTF-8 텍스트, 열별 아이콘과 가로 클리핑,
공급자 재진입·소스 변경, 잘못된 후보와 콜백 중 제거를
헤드리스 트리에서 검사한다. `LVUI.OwnedTable.EditingHost`와
`LVUI.OwnedTable.Generated`는 편집과 범위 붙여넣기, 취소, 검증 거절, 충돌,
프로그램에 의한 문자열 변경, 반응형 머리글 설정과 오류 복구, 위젯 제거를 검사한다.
`LVUI.OwnedTable.StructureHost`는 열 제거·재삽입과 그 과정의 공급자 재진입에서도
키별 아이콘이 일관되는지 검사한다. 이는 실제 창이나 스크린 리더 검증을 의미하지 않는다.
`LVUI.OwnedTable.AccessibleCells`는 셀 선택·편집, 비활성 및 오래된 셀, 병합 셀,
긴 UTF-8 값과 최대 길이 키, 기존 위젯 한도를 넘지 않는 가상 셀을 헤드리스로 확인한다.

키별 사용자 View의 부착·가시 범위·교체와 수명은 `iiGUI/TableViewGroup.h`가
관리한다. LVUI는 표 데이터의 관찰 가능한 컨텍스트와 그래픽 SDK에 독립적인
바인딩을 제공한다. 같은 키는 컨텍스트를 유지하고 제거된 키는 세션을 닫는다.
다른 스레드의 갱신은 컨텍스트를 만들기 전에 거부하며, 알림 중 그룹이 폐기되면
진행 중인 나머지 갱신을 중단한다. macOS arm64에서 기본·생성 표와 머리글, 스크롤·
실패 복구·재진입을 창 없이 검사했다.

표 머리글의 아이콘 준비·캐시와 셀별 클리핑·텍스트 및 아이콘 그리기는 iiGUI가
담당한다. LVUI는 표 디자인 값과 디코더 정책만 제공한다. 공급자 리비전이 같으면
바인딩 갱신 후에도 이미지를 재사용하며, 공급자가 변경 중이면 기존 화면을 보존한다.
