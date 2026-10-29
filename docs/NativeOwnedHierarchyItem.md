# LVUI 독립 계층 행

기본 `HierarchyItem`은 LVUI의 행 모델과 `NativeHierarchyItem`을 사용한다. 앱의
교체 가능한 아이콘 공급자와 범용 호스트의 입력·텍스트·그리기를 이용하며 Qt,
QML 또는 LVRS 위젯을 생성하지 않는다. [계층 소스의 보이는 행](OwnedHierarchyModel.md)은
`hierarchy_item_configuration()`으로 독립 행 설정에 변환할 수 있다.

행은 기본 200×20, 깊이당 8단위 들여쓰기, 18단위 아이콘, 16단위 접기 표시와
선택적 개수를 사용한다. 기본·호버·누름·선택·비활성·드래그 미리보기 색상과
치수를 변경할 수 있으며, 좁은 배치에서도 콘텐츠의 크기는 음수가 되지 않는다.
기본 아이콘이나 앱의 `IconSource`를 사용하고 공급자가 바뀌면 관측 가능한
`iconRevision`을 갱신한다. 잘못된 후보는 이전 행을 유지한다.
C++ 호스트는 이미 준비한 불변 래스터 아이콘을 직접 공급할 수도 있다. 이때 이름과
아이콘을 함께 지정하고 별도의 `IconSource`는 사용하지 않는다.

행을 누르면 `onClick`만 전달한다. 자식이 있는 행의 접기 표시를 누르거나 키보드
왼쪽·오른쪽을 사용하면 펼침을 변경하고 `onExpansionChanged(bool)`를 전달한다.
접기 표시 활성화 뒤에는 행 활성화도 요청한다. 콜백 중 행이 제거되거나 다른 설정이
게시되면 오래된 후속 콜백은 보내지 않는다. 동일한 값을 반응형으로 다시 전달하는
경우에는 후속 활성화를 유지한다. 비활성 행과 자식이 없는 행은 펼치지 않는다.
이름과 개수 설명은 접근성 정보로 제공하고 접기 표시는 별도의 동작을 제공한다.

선언형 입력은 `text`/`label`, `itemKey`, `itemId`, `count`, `indentLevel`,
`enabled`, `activatable`/`selectable`, `selected`, `expanded`, `showChevron`,
`hasChildItems`, `dragPreviewActive`, 아이콘·치수·패딩·색상과 두 콜백을 받는다.
`width`/`itemWidth`와 `height`/`rowHeight`는 각각 별칭이다. 지원하는 값은 반응형으로
갱신한다. 드래그 미리보기는 시각 상태만 의미한다.

`countView`에는 기본 숫자를 대신할 단일 View를 전달할 수 있다. 인라인 View,
재사용 View 호출과 `View` 입력을 통한 전달을 지원한다. View의 반응형 값과
콜백은 원래 스코프를 사용한다. `count`가 0 이상이고 `countWidth`가 양수일 때
표시하고, 그 밖에는 콘텐츠를 보존한 채 숨긴다. 영역의 크기 변경은 기존 View를
유지하며, 콘텐츠는 개수 영역 안으로 잘린다. 행을 비활성화하면 개수 View도 함께
비활성화된다. View를 사용해도 행의 접근성 설명에는
숫자 개수를 제공한다. `countView`를 생략하면 기존 숫자를 그린다.

```lvui
import vm LayerBackend
let Badge(text: String): View = Label(text: text)
Window {
    HierarchyItem(label: "Layers", count: vm.count, countWidth: 40,
                  countView: Badge(text: vm.badge), onClick: vm.activate)
}
```

개수 View에 암묵적인 행 객체를 주입하지 않는다. 필요한 데이터는 위 예제처럼
명시적으로 전달해야 한다. `[View]` 시퀀스나 필수 매개변수가 충족되지 않은 View는
소스 오류로 진단한다. `HierarchyItem`의 컨트롤러 주입과 드래그 제스처, 계층 목록이
생성하는 행의 사용자 대리자와 개수 View는 아직 지원하지 않는다. 실제 운영체제
보조 기술과 플랫폼별 렌더링 검증도 남아 있다.

C++ 앱은 `NativeHierarchyItem::create`의 마지막 `ii::gui::WidgetFactory` 인자로
같은 개수 영역을 제공할 수 있다. 팩터리는 구성 실패를 반환할 수 있으며, 기존
콘텐츠를 교체하는 호스트는 후보 구성이 성공한 뒤에만 새 행을 게시한다.

```text
import vm LayerBackend
Window {
    HierarchyItem(itemKey: "camera", label: vm.label, count: vm.count,
                  expanded: vm.expanded, hasChildItems: true,
                  onClick: vm.activate, onExpansionChanged: vm.setExpanded)
}
```

헤드리스 검사는 포인터·키보드·접근성 이름, 반응형 옵션과 잘못된 후보의 보존,
콜백 중 자기 제거를 다룬다. `LVUI.OwnedHierarchyItem.CountViewGenerated`는
전달된 View의 반응형 내용, 표시·숨김, 크기·정체성·수명을 검사한다. 생성 실패와
수명 경계는 앱이 소유한 트리 밖의
부수효과까지 되돌리지 않는다.
