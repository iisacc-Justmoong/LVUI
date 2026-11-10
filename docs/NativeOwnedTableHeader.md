# LVUI 독립 표 머리글

선언형 `TableHeader`는 [표 모델](OwnedTableModel.md)의 `TableColumn`과
`ii::gui::BasicTableViewportSource`를 입력으로 사용한다. iiGUI의 `BasicTableHeader`가 열 검증과
표시 범위·적중 판정을, `BasicNativeTableHeader`가 입력·접근성·그리기·위젯 수명을
소유한다. LVUI는 디자인 값과 바인딩을 제공하며 생성된 C++ 앱이 두 모듈을 조합한다.
두 모듈은 Qt, QML이나 LVRS 위젯에 의존하지 않는다.

```text
import vm TableModel
Window {
    TableHeader(columns: vm.columns, columnWidths: [120, 180], interactive: true, sort: vm.sort,
                columnAppearance: vm.columnAppearance, iconSource: vm.icons, iconRevision: vm.iconRevision,
                onColumnClicked: vm.columnClicked)
}
```

`columns`는 문자열 목록 또는 `std::vector<lvui::TableColumn>`을 받는다. 열과 폭은
반응형으로 갱신되며 폭의 개수가 열보다 많거나 값이 유효하지 않으면 마지막 정상
상태를 유지한다. 표시 위치 `offset`은 앱에서 지정할 수 있다.

`cellDelegate` 또는 `delegate`로 모든 머리글의 내용을 교체하고, `cellContext`로
읽기 전용 열 문맥의 이름을 정할 수 있다. 일부 열만 바꾸려면
`columnViews: [TableColumn(key: "0", header: Label(text: heading.text))]`와
`cellContext: "heading"`을 사용한다. 나머지 열은 기본 렌더링을 사용한다. 열 문맥의
필드와 가시 영역·수명 계약은 [표 호스트](NativeOwnedTable.md)와 같다.
`interactive`는 기본 머리글 클릭과 키보드 동작을 제어하며, 사용자 View 안의 버튼은
각자 입력을 처리한다. C++에서는 주입된 빌더의 `NativeTableViewsValue`에 있는
`header` 또는 `headers`를 iiGUI 호스트의 `create()` 마지막 인자로 전달한다.

기본 머리글은 폭 717, 행 높이 24, 구분선 1, 좌우 내용 여백 8, 세 개의 유연한 열을
사용한다. 명시적인 열 폭과 최소 폭, 선택적인 대체 폭 및 스크롤 위치를 지정할 수
있다. 열 키는 비어 있지 않고 고유해야 하며 사용하지 못하는 열은 활성화되지 않는다.
사용자는 `interactive`를 켜서 포인터 또는 왼쪽·오른쪽·Home·End 및 Enter·Space로
`column_clicked` 콜백을 받을 수 있다. 키보드 이동은 선택한 열을 가로 영역에 보이게 한다. 콜백에는 인덱스와 독립 `TableColumn` 값이
전달된다. 머리글은 선택한 열을 자동으로 정렬하지 않으며 정렬 정책은 앱이 소유한다.
팔레트만 변경할 때는 누르는 중인 열과 키보드 포커스를 유지한다. 열이나 상호작용
설정 등 다른 값이 바뀌는 경우의 입력 취소 규칙은 그대로 적용한다.

기본 선행 아이콘은 `generalchevronDown`, 후행 아이콘은 없음이며 크기는 18, 아이콘과
텍스트 사이 간격은 7이다. C++ `TableHeaderConfiguration`의
`default_leading_icon`, `default_trailing_icon`, `icon_size`, `icon_spacing` 또는
선언형의 `defaultLeadingIcon`, `defaultTrailingIcon`, `iconSize`, `iconSpacing`으로
변경한다. 빈 아이콘 이름 또는 크기 0은 해당 표시를 숨긴다. `column_appearance` 또는
선언형 `columnAppearance`는 `std::vector<lvui::TableHeaderColumnAppearance>`로 열 키,
선택적인 `padding`, `leading_icon`, `trailing_icon`을 지정한다. 값이 없는 항목은
공통 설정을 상속하며 아이콘에 명시한 빈 문자열은 공통 아이콘을 숨긴다.

```cpp
std::vector<lvui::TableHeaderColumnAppearance> columns{
    {.column = "name", .padding = 4, .leading_icon = "", .trailing_icon = "generalchevronUp"},
};
```

아이콘은 기본 LVUI 카탈로그 또는 `TableHeaderInput::icon_source`와 선언형
`iconSource`에 주입한 `lvui::IconSource`에서 찾는다. 사용자 공급자를 지정하면
기본 아이콘 이름도 그 공급자가 제공해야 하며 기본 카탈로그로 자동 대체하지 않는다.
가변 공급자는 자산이 바뀔 때
자신의 `revision()`과 반응형 `iconRevision`을 함께 갱신해야 한다. 같은 리비전의
아이콘은 재사용하고, 후보의 모든 열에서 아이콘을 준비한 뒤 화면을 교체한다.
화면 밖 열에 알 수 없는 아이콘이 있거나 공급자가 계속 변경되면 마지막 정상 상태를
보존한다. 서로 다른 아이콘의 2배 해상도 래스터 합계는 64 MiB로 제한한다. 이
계산에는 카탈로그와 호스트의 별도 메모리가 포함되지 않으며 교체 중 이전 이미지와
새 이미지가 동시에 유지될 수 있다. 크기 0이거나 모든 아이콘 이름이 비어 있으면
공급자를 조회하거나 아이콘을 디코딩하지 않는다.

아이콘 크기는 0 또는 1–256, 간격과 열별 여백은 0–8192여야 한다. 열별 설정의 키는
기존 열에 속하고 중복되지 않아야 하며, 이름은 공백만으로 구성되지 않은 최대
256바이트의 유효한 UTF-8이다. `iconRevision`은 0부터 2^53−1까지의 정수다. 가로
스크롤로 아이콘 일부가 가려지면 해당 영역을 잘라 원본 비율을 유지한다. 후행
아이콘은 네이티브 글자 폭을 계산해 텍스트 뒤에 배치하고 정렬 표시 영역과 겹치지
않는다. 아이콘은 장식이며 접근성 열 이름은 변경하지 않는다.

앱은 C++ `TableHeaderConfiguration::sort` 또는 선언형 `sort`에
`std::optional<lvui::TableHeaderSort>`를 전달해 현재 정렬 열의 키와
`lvui::TableSortOrder::ascending` 또는 `descending`을 표시할 수 있다. 예를 들어
`TableHeaderSort{"name", TableSortOrder::ascending}`은 `name` 열의 오름차순 표시다.
문자열 목록으로 열을 지정한 경우 키는 제목이 아니라 `"0"`, `"1"`처럼 0부터
시작하는 위치 문자열이다. 고정된 의미의 키가 필요하면 `TableColumn`을 전달한다.
비어 있는 값은 표시를 제거한다. 열을 클릭해도 값은 자동으로 바뀌지 않으며 앱이
데이터를 정렬하고 새 상태를 제공한다. 존재하지 않는 키나 잘못된 방향은 거절하고
마지막 정상 상태를 유지한다. 열 폭과 보이는 영역에 맞춰 표시를 자르며 정렬된 열의
지역화된 방향을 읽기 전용 머리글을 포함한 접근성 값에도 제공한다.

설정은 생성한 스레드에서 갱신하며 잘못된 열과 크기는 마지막 정상 상태를 유지한다.
화면에 보이는 각 열은 이름, 영역, 활성 상태와 동작 가능 여부를 가진 가상 접근성
요소이며 머리글 그룹에 속한다. 열 초점은 기존 키보드 위치에 연결되고 활성화는
기존 `column_clicked` 콜백을 호출한다. 비활성 열이나 `interactive=false`인 열은
활성화하지 않는다. 가로 스크롤과 이름 갱신 중 계속 보이는 열은 식별자를 유지하고,
화면에서 사라진 열은 퇴역한다. 제목은 UTF-8 경계를 지켜 최대 256바이트까지 제공한다.

`TableHeaderInput::text` 또는 선언형 `accessibilityText: vm.tableText`로
`lvui::TableText`를 전달해 그룹 이름과 제목 없는 열의 이름을 지역화할 수 있다.
자리표시와 유효성 조건은 [표 호스트](NativeOwnedTable.md)를 따른다. 정렬 방향은
지역화된 문자열 값으로 전달하며 운영체제의 구조화된 정렬 방향 속성은 아직 제공하지
않는다. 전체 `Table`의 열별 아이콘과 여백도 같은 표시 경로를 사용한다. 독립
머리글은 현재 없는 열 키를 거절하고, 전체 표는 열 삭제·재삽입을 위해 이를 보관한다.
실제 운영체제 렌더링과 보조 기술 검증은 별도로 남아 있다.

`LVUI.OwnedTableHeader.Model`, `Host`, `Codegen`, `Generated`는 폭 배분, 스크롤된 열, 유효성, 키보드·포인터
이벤트와 외부 정렬 상태·아이콘별 설정·스크롤 자르기·공급자 갱신 및 재진입·
래스터 한도·유효하지 않은 상태의 보존·반응형 갱신 및 수명을 창을 열지 않는
환경에서 검사한다.
`LVUI.OwnedTable.AccessibleHeaders`는 독립 머리글과 전체 표의 열 역할·동작·스크롤·
정렬 상태·지역화·오래된 데이터 및 콜백 중 퇴역을 검사한다.

C++ 앱은 `TableHeaderModelBinding`과 `TableHeaderBinding`에 iiGUI 모델·프레젠테이션·
사용자 View 그룹·호스트를 주입한다. 바인딩은 iiSDKCore 외 SDK를 직접 참조하지 않는다.
아이콘 준비·클리핑·텍스트 배치도 iiGUI가 소유한다. `iconRevision`은 갱신을 요청하는
신호이며 캐시의 실제 무효화는 공급자 `revision()`이 결정한다. 퇴역 중인 실행이 끝나면
프레젠테이션·자식 View·콜백을 반환한다. macOS arm64의 iiGUI 독립 설치 소비자와
LVUI 표·독립 머리글을 창 없이 검사했다.
