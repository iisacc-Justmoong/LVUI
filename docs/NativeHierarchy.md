# 네이티브 계층 뷰

> 2026-10-29: 이 문서의 `FRAMEWORK LVRS` 생성 경로와 LVRS 팩터리 검사 설명은
> 제거된 경로에 대한 역사적 기록이다. 현재 지원 범위와 남은 이식은
> [독립 구현 현황](LVUIIndependence.md)과 [이전 우선순위](NativeMigrationRoadmap.md)를 따른다.

`FRAMEWORK LVRS`의 `HierarchyItem`과 `HierarchyList`는 LVRS의 같은 이름인
자체 C++ 위젯을 생성한다. 기존 단순 ListModel 기반 `Hierarchy` 별칭과 구분한다.
실행 중 LVUI/QML 원문을 해석하지 않는다.

HierarchyList에는 native `HierarchyModel` 공유 핸들인 model/treeModel 또는
`HierarchyController` 공유 핸들인 controller 하나를 전달한다. 생략하면 빈 모델이다.
기본 폭 200·뷰포트 높이 400이며 부모 배치와 width/height를 따른다. 펼침·선택·탐색·
높이 인덱스와 이동 계획은 컨트롤러가 소유하며 뷰는 표시 범위만 유지한다.

```text
import vm Backend
Window {
    HierarchyList(controller: vm.tree, height: 240,
        generatedRowHeight: vm.rowHeight,
        generatedIconSize: 0,
        onActiveChanged: vm.selectLayer)
}
```

generatedRowHeight/rowHeight, generatedIndentStep/indentStep,
generatedIconSize/iconSize, generatedChevronSize/chevronSize 별칭을 받는다.
generatedItemWidth, 좌우 패딩·간격·라디우스·countWidth와 행 색상을 명시할 수 있다.
enabled·editable·keyboardNavigationEnabled·autoExpandAncestorsOnActivate를 지원한다.
editable/자동 펼침/기본 행 높이를 생략하면 명시적으로 전달한 컨트롤러의 정책을 보존한다.

HierarchyItem은 text/label, itemKey/itemId/count, indentLevel, selected/expanded,
activatable/selectable, enabled, showChevron/hasChildItems, iconName/iconGlyph,
크기·패딩·간격·라디우스와 원본 행 상태 색상을 받는다. count/ID/깊이는 언어의 정수
검사를 거치며 잘못된 수치·별칭 중복은 소스 위치로 진단한다.

표시 입력은 기본 반응형이며 유지된 네이티브 구성의 set_options로 전달한다. 기본 행 높이
변경은 view projection의 기본값을 바꾸고 개별 행 높이 override는 보존한다. 소스 데이터
revision이나 사용자 파일을 수정하지 않는다. model/controller의 반응형 공유 핸들이 바뀌면
새 네이티브 콘텐츠 세대를 준비하고 게시한다. 후보 행 생성에 실패하면 기존 뷰를 보존한다.
모델 소스는 같은 키의 선택을 이어받고 명시적 컨트롤러는 자신의 상태를 사용한다. 교체만으로
활성/펼침 콜백을 만들지 않으며 이전 소스의 관찰·행·진행 중 포인터 드래그를 정리한다.

onActiveChanged는 key:String·id:Int·index:Int에 대응하는 C++ payload,
onExpansionChanged는 key:String·expanded:Bool, onEnsureVisibleRequested는
y/height의 숫자, onItemMoved는 `const lvrs::HierarchyMovePlan&`를 전달한다.
독립 HierarchyItem은 onClick과 expanded:Bool 콜백을 제공한다. 실제 행 메타데이터는
필요할 때 컨트롤러에서 조회한다.

현재 AOT는 모델 기반 목록과 독립 행·HierarchyItem 루트 itemDelegate를 연결한다. 수동 자식 행,
원본 nodeData/model-role 변환, 모든 상속·모션·모바일 제스처·트리 접근성과
전체 동적 모델 role 호환성은 남은 범위이다. C++ API는 사용자 행 팩터리와 일반 countView 위젯 슬롯을
제공한다. unsupported 입력을 제거하거나 단순 List로 대체하지 않는다.

## 행의 LVUI 사용자 콘텐츠

HierarchyItem의 countView는 인라인 View·기존 View 값·타입 지정 View 구성을 받는다.
첫 매개변수는 별칭으로 가져온 `lvrs.HierarchyItem` 타입이며 현재 행의 C++ 핸들을 받는다.
추가 매개변수는 기본값을 가져야 한다. 매개변수 없는 View와 명시적으로 호출한 View도
사용할 수 있다. `[View]`는 직접 전달하지 않으며 복수 컨트롤은 스택으로 묶는다.

```lvui
import class Row lvrs.HierarchyItem
import vm Backend
let Controls(row: Row, caption: String = "Select"): View = HStack {
    Label(text: vm.describe(row))
    PushButton(row.activate, text: caption)
}
Window { HierarchyItem(count: vm.count, countView: Controls) }
```

타입 청사진은 인스턴스를 만들지 않으며 기존 행을 입력으로 빌린다. countView는 별도
위젯 ID 영역·Bindings·지역 import 수명을 가진다. 같은 스코프에서 서로 다른 행에
같은 View를 써도 ID가 충돌하지 않는다. 부모 백엔드는 차용하고 지역 import만 생성한다.
일반 조회는 행의 옵션 getter 안의 Core 관측을 수집하며 `@Static`은 해당 활성화의
초기값을 보존한다. 조회·표시 변경으로 모델/파일 상태를 쓰지 않는다.

count가 음수이면 콘텐츠를 생성하지 않고, 0 이상이 되면 생성한다. 표시 중 값 갱신은
같은 콘텐츠·편집 입력을 유지하며, 음수로 돌아가면 구독·지역 import를 퇴역시킨다.
재표시는 새 활성화이다. 부모 퇴역이나 슬롯 퇴역 뒤에는 이전 위젯 명령을 받지 않는다.
슬롯이 자기 명령 중 제거되면 기존 콜백의 실행 중 소유자 보존 계약을 따른다.

초기 콘텐츠 생성 실패는 후보 구성 오류로 반환한다. 숨겨진 콘텐츠를 여는 설정이 실패하면
이전 행 옵션·표시와 빈 슬롯을 보존하여 같은 설정으로 재시도할 수 있다. 해당 행의 구성
중 재진입 set_options는 busy이다. 사용자 팩터리의 외부 부수효과는 롤백하지 않는다.
HierarchyList의 수동 행과 전체 delegate 정책은 별도 남은 계약으로 유지한다.

## 목록의 itemDelegate

`itemDelegate`는 HierarchyItem을 루트로 하는 View를 받는다. 첫 입력은 가져온
`lvrs.HierarchyDelegateContext` 타입이며, 생략 가능한 추가 입력에는 기본값을 지정한다.
매개변수 없는 인라인 View와 기존 View 슬롯 전달도 사용할 수 있다.

```lvui
import class Entry lvrs.HierarchyDelegateContext
import vm Backend
let Layer(entry: Entry): View = HierarchyItem(
    label: vm.title(entry), baseLeftPadding: 4, iconSize: 0)
Window {
    HierarchyList(controller: vm.tree, itemDelegate: Layer)
}
```

생략한 행 입력은 실제 소스 옵션에서 상속한다. 명시한 표시 값은 소스가 갱신되거나
관련 백엔드 값이 바뀔 때 다시 계산하며 실제 행의 같은 네이티브 핸들에 적용한다.
목록이 사용자 표시를 기본 행 옵션으로 덮어쓰지 않는다. 초기 생성·설정 실패는 오류로
반환하고 후속 계산 실패는 마지막 정상 표시와 복구 의존성을 유지한다.

키/ID·깊이·컨트롤러·선택/펼침·자식 여부·드래그 상태와 폭/높이는 목록이 소유한다.
delegate에서 이 입력을 지정하면 진단하며, 행 높이는 목록 설정 또는 컨트롤러 override로
조작한다. 이는 원본의 모든 동적 기하/모델-role 계약을 완료한 것으로 해석하지 않는다.
지역 import는 가시 범위의 각 키에 한 번 생성하고 유지된 행의 선택/표시 갱신은 재생성하지
않는다. 화면 밖 제거와 소스 교체는 해당 행과 내부 countView의 수명을 함께 종료한다.

기본 목록 이벤트는 사용자 행 이벤트와 연결된다. 사용자 클릭 콜백을 추가해도 목록의
선택 명령을 대체하지 않으며, countView의 별도 버튼은 자신의 이벤트 범위를 유지한다.
전체 계약은 [LVRS delegate 문서](../../LVRS/docs/native/HierarchyDelegate.md)에 기술한다.

onExpansionChanged는 벌크 펼침/접기와 자동 조상 펼침의 실제 변경마다 호출한다.
벌크는 preorder, 자동 조상은 가까운 부모부터 루트 순서이다. 모든 콜백은 정규화한 최종
컨트롤러 상태를 조회하며 펼침 콜백 뒤에 활성 콜백을 전달한다. 공유 컨트롤러에 명시하는
정책 설정과 사용자 C++ 팩터리의 외부 부수효과는 후보 실패의 롤백 대상이 아니다.

`tests/hierarchy.cpp`는 코드 생성·반응형 설정·콜백·진단을 검사한다. 실제 생성 팩터리
컴파일과 LVRS의 창 없는 위젯 검사는 별개이며 LVRS 실행은 전체 목표의 마지막 단계에
모은다. 자세한 동작은 [LVRS HierarchyViews](../../LVRS/docs/native/HierarchyViews.md)에 있다.

이전 hierarchy-views 체크포인트에서 일반·ASan/UBSan의 관련 LVUI 검사 74개씩은 통과했다. 이 기능을 포함한 생성 팩터리
69개와 재배치 소비자의 12개 팩터리는 두 구성에서 컴파일·링크했다. LVRS 관련 117개씩은
실행 파일과 등록을 대조했으며 실행하지 않았다. 결과는 LVRS 소스 저장소의
`build/lvui-completion/hierarchy-views/verification-20261027.json`에 구분하여 기록한다.


후속 hierarchy-sources 단계에서는 반응형 controller 핸들의 교체·복원을 실제 생성
팩터리에 추가하고, 모델 교체 실패 보존·화면 밖 선택·소스 getter 의존성·콜백 오류를
LVRS 검사에 준비한다. LVRS 관련 등록은 135개이며 실행은 최종 일괄 단계에 남긴다.
최종 빌드·설치·생성물 대조 기록은 LVRS 저장소의
`build/lvui-completion/hierarchy-sources/verification-20261027.json`이다.


hierarchy-slots 체크포인트(2026-10-27)는 countView의 행 정보·지역 import·실행 중 자기
제거·숨김/재활성화·편집 입력 보존을 생성 C++ 검사에 추가했다. LVUI 전체 검사는 일반과
ASan/UBSan 각각 295개를 통과했다. 생성 팩터리 70개와 재배치 소비자 13개는 두 구성에서
컴파일·링크하며, LVRS 143개는 실행 파일·등록을 대조하고 최종 일괄 실행에 남긴다.
최종 기록은 LVRS의 `build/lvui-completion/hierarchy-slots/verification-20261027.json`이다.


후속 hierarchy-delegates 체크포인트(2026-10-27)는 소스/선택 갱신·사용자 스타일·내부 입력
보존·가상화·소스 교체·실패 복구를 생성 C++ 검사에 추가했다. LVUI 전체 299개는 일반과
ASan/UBSan에서 각각 통과했다. 생성 팩터리 71개와 재배치 소비자 14개를 두 구성에서
컴파일·링크하고 LVRS 170개는 빌드·등록 후 최종 실행 단계에 남긴다. 기록은 LVRS의
`build/lvui-completion/hierarchy-delegates/verification-20261027.json`이다.
