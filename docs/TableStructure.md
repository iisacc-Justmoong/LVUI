# 표 구조와 크기 편집

`TableStructure.h`는 행·열 삽입과 삭제, 행 높이와 열 너비 변경을 교체 가능한
`TableStructureEditor` 경계로 제공한다. `TableStructureEdit`은 관찰한 문서 리비전과
최대 256개 연산을 순서대로 담는다. `prepare_table_structure()`는 불변 스냅샷에
후보를 만들고 각 단계의 키, 셀 수, 크기와 전체 데이터 제한을 검사한다. 준비 중에는
원본이 바뀌지 않는다.

행 삽입은 현재 열 수와 같은 셀을 제공한다. 열 삽입은 현재 각 행에 추가할 셀을
제공한다. 삭제·크기 변경은 행이나 열의 키로 대상을 지정한다. 중복·누락 키, 잘못된
높이·너비, 잘못된 셀 수와 오래된 리비전은 전체 요청을 거절한다. 선택적
`TableMemoryStructureEditor`는 모든 연산을 검사한 뒤 하나의 스냅샷 교체로 게시한다.
변화가 없으면 리비전을 발행하지 않는다. 다른 저장소는 같은 인터페이스에 자체 검증과
원자적 게시를 구현할 수 있다. 메모리 어댑터는 표 전체를 복사하므로 큰 영속 표에는
앱에 맞는 편집기를 사용하는 편이 적합하다.
행·열 크기 변경은 [병합 범위](TableMerge.md)를 유지하고 삽입·삭제는 위치가 바뀌는
병합을 해제한다. 실제 값은 각 셀에 보존한다.

[독립 표 호스트](NativeOwnedTable.md)에 `structureEditor`를 주입하면 C++ 앱은
`apply_structure()`로 구조 변경을 요청할 수 있다. `columnResizeEnabled`와
`rowResizeEnabled`를 켜면 헤더의 열 경계와 본문의 행 경계를 끌어 크기를 조절한다.
드래그 중에는 미리보기만 그리며 놓을 때 한 번 확정한다. Escape, 설정 변경 또는
포커스 이탈은 미리보기를 취소한다. 드래그 중 외부 문서 변경이 발생하면 오래된
크기로 덮어쓰지 않는다. 열은 최소 32, 행은 최소 18 논리 단위다. 유연 너비 열을
조절하면 확정한 고정 너비가 된다. 경계 너비는 `columnResizeHandleWidth`와
`rowResizeHandleHeight`로 설정할 수 있다.

실제로 변경한 구조에는 `onStructureChanged`가 요청 전체를, 드래그로 확정한 크기에는
`onColumnResized` 또는 `onRowResized`가 인덱스와 크기를 전달한다. 삽입·삭제도 같은
편집기에 전달하므로 영속 저장과 Undo/Redo는 앱에서 주입한 편집기의 책임이다.
삭제로 선택한 키가 사라지면 선택을 비우고 `onSelectionChanged`에도 이를 전달한다.

`structureEditor`와 `structureControlsVisible: true`를 지정하면 각 보이는 행의 오른쪽과
열의 아래에 추가 조작이 나타난다. 누른 행이나 열 바로 뒤에 삽입하며, 양축 스크롤과
함께 대상 위치가 바뀐다. `addRowControlsVisible`, `addColumnControlsVisible`로
각 축을 숨기고 `structureGutterWidth`, `structureGutterHeight`로 폭과 높이를
설정한다. 두 조작은 기본 20 논리 단위이며 허용 범위는 1–256이다. 이 영역을 숨기면
남은 공간은 표 표시 범위로 돌아간다. 추가 영역과 그 모서리는 표의 배경색을 사용한다.
구조 조작 전체는 기본적으로 숨겨져 있다.

```text
Table(model: vm.controller, structureEditor: vm.structure,
      structureFactory: vm.factory, structureControlsVisible: true,
      onStructureChanged: vm.structureChanged)
```

구조 편집기가 있으면 기본적으로 활성 셀의 컨텍스트 메뉴에서 행 또는 열 삭제를 요청할 수
있다. `deleteContextMenuEnabled: false`로 끄며, `selectOnContextClick: false`로
메뉴를 열 때 셀을 선택하지 않도록 할 수 있다. 마지막 행은 삭제할 수 있지만 마지막
열은 삭제할 수 없다. 행 또는 열이 없으면 해당 추가 영역의 첫 위치에 더하기를
표시하며, 완전히 빈 표에서도 행과 열을 어느 순서로든 추가할 수 있다. 첫 항목용
동작 표면은 각각 `Add first row`, `Add first column`이라는 기본 접근성 이름과
버튼 역할을 갖는다. 기존 행과 열에 대응하는 표시도 각각 버튼 역할과
`Add row after row 1`, `Add column after column 1` 형태의 기본 접근성 이름을 갖는다.
현재 화면에 보이는 항목만 노출하며 일반 경로에서는 위치나 인덱스가 바뀌어도 같은
키의 버튼을 유지하고 이름과 위치를 갱신한다. 화면 밖으로 나가면 해당 버튼은 퇴역한다. Tab으로
이동하고 Enter·Space 또는 접근성 활성화로 조작할 수 있으며 포커스 표시를 제공한다.
기본 이름은 영어이며, 이 이름의 별도 지역화 입력은 아직 없다. 표나 해당 추가 영역이
비활성·숨김 상태일 때는 동작하지 않는다. 비활성 표는 편집하지 않고 포인터를
누른 뒤 다른 위치에서 놓으면 삽입하지 않는다. 동작을 시작한 뒤 모델이나 뷰 설정이
바뀌면 이전 대상에 대한 요청을 거부한다.

중첩 깊이나 공유 위젯 수가 키별 활성화를 수용하지 못하면 같은 버튼을 한 콘텐츠
세대로 묶는 경로를 사용한다. 이 경로에서도 보이는 키 순서가 같으면 스크롤 위치만
갱신하고 버튼을 유지한다. 보이는 항목의 집합이나 순서가 바뀌면 묶음을 다시 만들어
기존 버튼의 접근성 포커스가 해제될 수 있다. 더 작은 묶음도 호스트의 공유 위젯 한도에
들어가지 않으면 오류를 반환한다.
선택 기능이나 컨트롤러의 선택 모드를 끈 경우에도 삭제 메뉴는 선택을 만들지 않고 열린다.

기본 `TableDefaultStructureFactory`는 빈 셀과 현재 표 안에서 중복되지 않는
`lvui-row-N`, `lvui-column-N` 키를 만든다. 삭제된 과거 키까지 영구적으로 예약하지는
않는다. 앱이 영속적인 ID나 도메인별 기본 셀을 사용하려면 `TableStructureFactory`의
`make_row()`와 `make_column()`을 구현하고 `structureFactory`로 주입한다. 팩터리는
생성할 데이터만 제공하고, 대상과 단일 수정 연산은 호스트가 결정한다.
`plan_table_structure_action()`은 키와 리비전으로 후보를 만들고 데이터와 셀 수를
검증하지만 원본을 수정하지 않는다. 빈 축에 대한 `insert_first_row`와
`insert_first_column`은 빈 키와 빈 해당 축을 요구하고 팩터리에 위치 0을 전달한다.
이미 항목이 있거나 키가 들어온 첫 항목 요청은 팩터리를 호출하기 전에 거절한다.
앱의 `TableStructureEditor`가 최종 원자적 게시와
충돌 검사를 담당한다.

`LVUI.OwnedTable.Structure`는 혼합 연산의 후보·원자성, 데이터 정렬, 읽기 스냅샷,
잘못된 요청과 동시 변경을 검사한다. `LVUI.OwnedTable.StructureHost`와 생성 코드
검사는 크기 미리보기·확정·취소, 외부 변경과 반응형 콜백을 헤드리스 트리에서
확인한다. `LVUI.OwnedTable.StructureActions`, `StructureControls`,
`StructureControlsGenerated`는 삽입·삭제, 교체 가능한 팩터리, 스크롤된 대상, 비활성
상태와 오래된 요청 및 선언형 연결을 검사한다. `EmptyControls`는 두 축의 첫 삽입,
팩터리 오류 복구, 포인터·키보드·접근성 활성화, 비활성·숨김과 오래된 요청을
검사하며 마지막 행 삭제 후 선택을 정리하고 첫 추가 버튼을 다시 표시하는지도 확인한다.
`AccessibleStructure`는 기존 행·열 버튼의 이름·키보드와 접근성 활성화, 키 보존,
스크롤·숨김·비활성·오래된 요청과 깊이·위젯 한도에 따른 대체 경로를 창 없이 검사한다.
실제 운영체제의 포인터·터치 동작과 보조 기술은 별도
검증이 필요하다.
