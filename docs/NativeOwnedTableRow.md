# LVUI 독립 표 행

`TableRowView`는 키가 있는 셀과 행의 가로 배치를 소유하며,
`NativeTableRow`는 보이는 [독립 표 셀](NativeOwnedTableCellItem.md)만 조합한다.
Qt, QML, LVRS 위젯을 사용하지 않는다. C++에서 `TableRowInput`과 셀 키를 전달하거나 기본 선언형 `TableRow`를 사용한다.

```text
import vm RowModel
Window {
    TableRow(cells: vm.cells, cellWidth: 100, inputable: true,
             onCellInputEdited: vm.edited, onCellInputSubmitted: vm.submitted)
}
```

`cells`는 문자열 목록, `std::vector<lvui::TableCell>` 또는 명시적 키와 검증기를 가진
`std::vector<lvui::TableRowCell>`을 받는다. 앞의 두 형태는 인덱스를 키로 사용하므로
셀 순서를 바꾸면서 동일한 편집 상태를 유지하려면 명시적 키를 제공한다.

기본 폭은 717, 셀 폭은 234, 높이는 24다. 공간이 남으면 셀 사이에 균등하게 배분하고
공간이 부족하면 가로 `offset`에 따라 보이는 셀만 유지한다. 각 `TableRowCell`은 고유한
키, 독립 `TableCell` 값과 선택적인 검증기를 가진다. 키가 유지되는 표시 셀은 내용이나
순서가 바뀌어도 위젯을 재사용한다. 화면 밖으로 벗어난 셀은 제거되며 해당 편집은
취소된다. 최대 1,024개 셀과 총 4 MiB의 셀 텍스트를 허용한다.

셀 편집·제출·거부는 현재 행 인덱스와 값을 콜백으로 전달한다. 앱이 편집 초안을
반응형으로 다시 제공하면 해당 초안을 유지한다. 검증과 외부 변경 충돌 처리는 개별
셀과 동일하다. 입력 콜백 도중 표시 셀의 구성이 바뀌면 위젯 갱신을 다음 배치로 미뤄
처리한다. 열별 접근성 요소와 실제 운영체제 문자 입력 및 렌더링 검증은 별도로 필요하다.

`LVUI.OwnedTableRow.Model`, `Host`, `Codegen`, `Generated`는 배치와 가상화, 키 보존, 값 갱신·편집과 스레드
및 수명 경계를 창을 열지 않는 위젯 트리에서 검사한다.
