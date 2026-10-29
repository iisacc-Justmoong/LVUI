# LVUI 독립 표 머리글

`TableHeader`는 독립 [표 모델](OwnedTableModel.md)의 `TableColumn`과
`TableViewportSource`를 사용해 고정 폭과 유연한 열의 너비, 가로 스크롤과 보이는
열을 계산한다. `NativeTableHeader`는 이 값을 범용 iiGUI 입력 표면에서 그리며 Qt,
QML, LVRS에 의존하지 않는다. C++ API와 기본 선언형 `TableHeader`를 모두 사용할 수 있다.

```text
import vm TableModel
Window {
    TableHeader(columns: vm.columns, columnWidths: [120, 180], interactive: true,
                onColumnClicked: vm.columnClicked)
}
```

`columns`는 문자열 목록 또는 `std::vector<lvui::TableColumn>`을 받는다. 열과 폭은
반응형으로 갱신되며 폭의 개수가 열보다 많거나 값이 유효하지 않으면 마지막 정상
상태를 유지한다. 표시 위치 `offset`은 앱에서 지정할 수 있다.

기본 머리글은 폭 717, 행 높이 24, 구분선 1, 좌우 내용 여백 8, 세 개의 유연한 열을
사용한다. 명시적인 열 폭과 최소 폭, 선택적인 대체 폭 및 스크롤 위치를 지정할 수
있다. 열 키는 비어 있지 않고 고유해야 하며 사용하지 못하는 열은 활성화되지 않는다.
사용자는 `interactive`를 켜서 포인터 또는 왼쪽·오른쪽·Home·End 및 Enter·Space로
`column_clicked` 콜백을 받을 수 있다. 키보드 이동은 선택한 열을 가로 영역에 보이게 한다. 콜백에는 인덱스와 독립 `TableColumn` 값이
전달된다. 머리글은 선택한 열을 자동으로 정렬하지 않으며 정렬 정책은 앱이 소유한다.

설정은 생성한 스레드에서 갱신하며 잘못된 열과 크기는 마지막 정상 상태를 유지한다.
그리는 열 제목은 UTF-8 경계를 지켜 제한한다. 현재 머리글 전체가 하나의 접근성 입력
표면이며 열마다 별도 접근성 요소를 제공하거나 정렬 표시 및 사용자 정의 열은 아직 필요하다. 실제 운영체제 렌더링과
보조 기술 검증도 수행하지 않았다.

`LVUI.OwnedTableHeader.Model`, `Host`, `Codegen`, `Generated`는 폭 배분, 스크롤된 열, 유효성, 키보드·포인터
이벤트와 갱신 및 수명을 창을 열지 않는 환경에서 검사한다.
