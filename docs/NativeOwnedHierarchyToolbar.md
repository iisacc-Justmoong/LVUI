# LVUI 독립 계층 도구 모음

상위 복합 뷰의 게시 중 버튼 콘텐츠 게시가 일시적으로 차단되면 다음 배치에서 다시
시도해 상위 화면의 게시가 끝난 뒤 버튼을 표시한다.

`HierarchyToolbar`는 LVUI의 도구 버튼과 교체 가능한 항목 목록으로 구성한다. 항목의
선택과 탐색은 도구 모음의 화면 상태이며 앱 문서를 변경하지 않는다. Qt, QML과 LVRS
위젯을 생성하지 않는다.

```text
import vm ToolbarBackend
Window {
    HierarchyToolbar(buttonItems: vm.items, activeButtonId: vm.active,
                     onActiveChanged: vm.selectionChanged,
                     onButtonTriggered: vm.toolTriggered)
}
```

`buttonItems`는 `lvui::HierarchyToolbarItem` 벡터 또는 그 공유 포인터다. 항목은
정수·문자열 ID, 아이콘 이름, 텍스트, 접근성 이름, 활성·표시 상태, 초기 선택과
선택적인 `HierarchyToolbarCommand`를 가진다. 명시 ID가 없으면 현재 항목 번호를
사용한다. ID는 목록 안에서 고유해야 한다. 기존 선택 ID가 보이고 활성 상태라면
목록 갱신 뒤에도 유지한다. 그렇지 않으면 보이는 활성 항목 중 초기 선택 항목,
그다음 첫 항목을 선택한다. 선택 가능한 항목이 없으면 선택은 비어 있다.

`activeButtonId`는 앱이 선택을 제어할 때 사용할 수 있는 정수·문자열 또는 선택적
ID다. `onActiveChanged`와 `onButtonTriggered`는 ID와 현재 인덱스가 담긴
`HierarchyToolbarSelection`을 전달한다. 사용자 동작은 선택이 달라진 경우 선택 변경,
작동 콜백, 항목 명령 순서로 전달한다. 콜백 중 항목이나 표시 설정이 바뀌거나 모음이
제거되면 오래된 후속 동작을 중단한다. 앱이 같은 선택 ID를 반응형 상태로 확인하는
경우에는 유효한 작동을 계속 전달한다. 좌우·Home·End 키는 숨기거나 비활성화한 항목을
건너뛰며 선택을 바꾼다.

기본 최소 폭은 200, 슬롯 높이는 22, 아이콘 크기는 18, 가로 여백은 8, 세로 여백은
2다. 기본 배경 불투명도는 0이다. `distributeSpacing`을 켜면 남는 폭을 보이는 슬롯
사이에 나눈다. 좁은 폭에서는 슬롯이 영역 밖으로 나오지 않게 축소한다. 살아 있는 ID의
버튼은 항목 재정렬과 속성 갱신을 거쳐 유지되며, 삭제한 항목은 제거된다. 잘못된 ID,
아이콘, 치수나 중복 항목은 기존 도구 모음을 보존한다. 최대 256개 항목과 1 MiB의
항목 텍스트를 허용한다.

C++ 앱은 독립 모델 `HierarchyToolbar`, `HierarchyToolbarInput`,
`NativeHierarchyToolbar`와 명령 인터페이스를 직접 사용할 수 있다. 아이콘 공급자는
`iconSource`와 `iconRevision`으로 교체한다. 현재 선언형 경로는 `buttonItems`만
받으며 직접 자식 버튼 등록과 개별 이름 이벤트·페이로드는 아직 지원하지 않는다.
실제 운영체제 표시와 보조 기술 동작도 별도 검증이 필요하다.

`LVUI.OwnedHierarchyToolbar.Model`은 ID와 선택 정규화, 탐색, 크기와 잘못된 입력을
검사한다. 호스트와 생성 코드 검사는 키 기반 버튼 유지, 반응형 선택·실패 복구,
키보드와 콜백 순서, 수명 경계를 헤드리스 트리에서 확인한다.
