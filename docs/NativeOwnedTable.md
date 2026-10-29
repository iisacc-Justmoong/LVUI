# LVUI 독립 표 호스트

`NativeTable`은 [독립 표 소스와 선택 컨트롤러](OwnedTableModel.md)를 범용 iiGUI
스크롤·입력·그리기에 연결한다. 셀을 렌더링할 때 LVUI가 양축 표시 범위, 열과 행
위치, 색상, 격자 및 선택 영역을 직접 계산한다. Qt, QML과 LVRS 위젯은 사용하지
않는다. C++ 앱은 `TableInput::controller`에 `std::shared_ptr<TableController>`를
넣고 `NativeTable::create()`로 조합할 수 있다. 기본 선언형 `Table`도 같은 경로를
생성한다.

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
동작과 충돌 처리는 [표 구조와 크기 편집](TableStructure.md)을 따른다.

기본 크기는 660×300, 헤더와 행의 기본 높이는 24, 셀 좌우 여백은 8이다. 열의
명시적 너비는 최소 32이며 너비가 0인 열은 남은 폭을 나누거나 설정된 기본 너비를
사용한다. 행 높이는 소스가 지정한다. 헤더는 세로로 고정되고 가로 스크롤은 본문과
공유한다. 화면 밖 행과 열의 셀을 그리지 않으며, 내용 삽입 뒤에는 가능할 때 각 축의
보이는 키와 내부 위치를 유지한다. 사용자가 움직인 축에는 이전 기준점을 적용하지 않는다.

포인터를 누르거나 끌어 사각 범위를 선택하고, 방향키·Home·End와 Shift로 셀을
이동하거나 범위를 확장할 수 있다. 비활성 셀은 선택하지 않는다. `selection_changed`는
키 기반 선택을 전달한다. 화면 선택과 스크롤은 문서 리비전을 변경하지 않는다.
앱이 소스를 교체하면 `source_revision`을 변경해 갱신한다. 이벤트 직전에 소스가
바뀌었다면 오래된 이벤트를 전달하지 않고 새 화면을 만든다. 설정이나 새 화면의
검증 실패는 마지막으로 유효한 위젯을 보존한다. 위젯과 컨트롤러는 생성한 스레드에서
사용한다.

한 화면에서 그리는 셀은 호스트의 그리기 예산에 맞춰 최대 3,072개이며 이를 초과하는
후보는 오류로 거절한다. 긴 셀 텍스트는 화면 그리기 예산 안에서 UTF-8 경계를 지켜
앞부분을 표시하고 원본 데이터는 보존한다. 운영체제 보조 기술에는 현재 표 전체의
이름만 제공한다. 셀별 접근성, 셀 병합, 행·열 삽입·삭제 UI 및 실제 플랫폼
렌더링은 아직 구현하지 않았다. 미지원 선언은
컴파일 오류로 알려준다.

`LVUI.OwnedTable.Host`는 1,000행의 가상 범위와 두 축 스크롤, 그리기, 포인터·키보드
선택, 오래된 이벤트, 비활성 셀, 긴 UTF-8 텍스트, 잘못된 후보와 콜백 중 제거를
헤드리스 트리에서 검사한다. `LVUI.OwnedTable.EditingHost`와
`LVUI.OwnedTable.Generated`는 편집과 범위 붙여넣기, 취소, 검증 거절, 충돌,
프로그램에 의한 문자열 변경, 반응형 갱신과 위젯 제거를 검사한다. 이는 실제 창이나 스크린 리더
검증을 의미하지 않는다.
