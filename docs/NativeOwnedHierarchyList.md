# LVUI 독립 계층 목록

`NativeHierarchyList`는 LVUI가 소유한 계층 투영과 행 구현을 범용 iiGUI 스크롤
호스트에 연결한다. Qt, QML, LVRS 위젯을 사용하지 않는다. 읽기와 쓰기는
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

고정 높이 행의 보이는 범위와 `overscan`만 마운트한다. 스크롤 중 계속 보이는 키는
행 객체와 입력 상태를 재사용하고, 범위를 벗어난 행은 해제한다. 소스 삽입과 제거
후에는 가능한 한 이전의 보이는 키와 행 내부 위치를 유지한다. 소스나 설정의
잘못된 후보, 존재하지 않는 아이콘은 이전에 표시된 내용을 유지하고 오류를 돌려준다.
복구된 값을 전달하면 다시 갱신된다. 스냅샷이 게시되기 전에 외부 앱이 변경한 상태나
이미 전달한 콜백을 되돌리지는 않는다.
상위 복합 뷰가 게시되는 동안 행 콘텐츠 게시가 일시적으로 막히면, 상위 게시가
완료된 뒤 다음 레이아웃에서 보이는 행을 다시 구성한다.

선언형 입력은 `controller`, `modelRevision`, 크기, `rowHeight` 또는
`generatedRowHeight`, `overscan`, `enabled`, `showIndicator`,
`keyboardNavigationEnabled`, 아이콘 공급자와
`iconRevision`, 들여쓰기·패딩·아이콘·접기 표시·개수 크기 및 행 색상을 받는다.
`generatedIndentStep`/`indentStep`, `generatedIconSize`/`iconSize`,
`generatedChevronSize`/`chevronSize`는 각각 별칭이다. 값은 반응형으로 갱신된다.
아이콘 공급자를 바꿀 때는 `iconRevision`도 갱신한다. `editable: true`일 때는
같은 계층 소스를 편집하는 `std::shared_ptr<lvui::HierarchyEditor>`를 `editor`로
제공한다. 컨트롤러와 위젯은 생성한 스레드에서 사용한다.

현재 행을 클릭하고 접기 표시 또는 행의 좌우 키로 펼칠 수 있다. 위·아래, Home,
End, Page Up과 Page Down 키는 선택할 수 없는 항목을 건너뛰고 대상 행을 표시한 후
포커스를 옮긴다. 오른쪽 키는 펼친 행에서 자식으로, 왼쪽 키는 접힌 행에서 부모로
이동한다. 키보드 탐색은 끌 수 있다. 편집 가능한 행은 포인터로 끌 수 있으며
원본·대상 행을 미리 표시하고 놓는 순간에만 리비전이 포함된 이동을 요청한다.
콜백 `onItemMoved`는 성공한 `lvui::HierarchyMovePlan`을 받는다. 편집이 성공한
뒤 새 화면을 만들지 못하더라도 성공한 이동은 콜백으로 전달하고 화면 오류를
호출자에게 알린다. 일반 클릭과 펼침 표시는 유지하고, 소스 변경·취소·편집 비활성화 시 진행 중인 후보를 버린다.
가장자리 스크롤은 포인터 이동 이벤트당 한 행씩 진행한다. 사용자 행 대리자와
개수 뷰, 가변 행 높이 및 운영체제 보조 기술 연결은 아직 구현하지 않았다.
미지원 선언은 컴파일 오류로 알려준다.

`LVUI.OwnedHierarchyList.Host`는 1,000개 항목의 가상화, 키별 재사용과 해제,
소스 비변경, 삽입 후 기준점, 오래된 이벤트, 잘못된 아이콘 복구 및 콜백 중 제거를
헤드리스 트리에서 검사한다. `LVUI.OwnedHierarchyList.Keyboard`은 방향·페이지 이동,
스크롤과 포커스, 부모·자식 이동, 비활성 항목 및 오래된 키 이벤트를 검사한다.
`LVUI.OwnedHierarchyList.Drag`는 클릭과 드래그 구분, 하위 트리 이동, 오래된
데이터 취소, 편집 거절, 콜백 중 제거와 접기 표시를 검사한다. 생성 코드 검사와
헤드리스 생성 위젯 검사도 제공한다.
이는 실제 창, 플랫폼 접근성 또는 기기 렌더링 검증을 의미하지 않는다.
