# LVUI 독립 카드

`Card`는 LVUI가 소유한 카드 데이터, 상태와 배치 모델이고 `NativeCard`는 이를 범용
iiGUI 입력·그리기 표면에 연결한다. 기존 라이브러리의 위젯이나 QML을 사용하지 않는다.
앱은 `CardInput`에 값과 선택적인 래스터 아이콘·미리보기 이미지를 전달하거나 아이콘
이름과 교체 가능한 `IconSource`를 지정한다. `NativeCard::create()`가 반환하는 카드의
`surface()`, `menu()`, `action()`은 각각 별도 입력 표면이다. 기본 선언형 경로도
같은 자체 구현을 사용한다.

```text
import vm ProjectCardModel
Window {
    Card(type: .Project, title: vm.title, rows: vm.rows, progress: vm.progress,
         selected: vm.selected, onSelectedChanged: vm.selection,
         onClick: vm.open, onMenuRequested: vm.menu, onActionTriggered: vm.action)
}
```

선언의 `rows`는 `std::vector<lvui::CardRow>`를 전달하고 `previewSource`와 `icon`은
네이티브 래스터를 받는다. `iconName`과 `iconSource`는 교체 가능한 아이콘 공급자를
사용한다. 텍스트, 상태, 크기, 색상과 이미지가 바뀌면 기존 카드를 갱신한다.

파일, 파일 미리보기, 폴더, 프로젝트, 장치, 모델, 멤버, 링크 유형을 모델에서 표현한다.
파일 카드는 크기에 따라 140×160, 210×240 또는 360×280이며 미리보기는 480×320,
다른 유형은 기본 폭 256과 최소 높이 280을 사용한다. `width`와 `height`는 이 크기를
재정의한다. 미리보기 이미지는 가운데를 기준으로 영역에 맞추고, 프로젝트와 장치는
진행 표시를 사용한다. 행은 최대 128개, 전체 텍스트는 2 MiB로 제한한다. 표시와
접근성 텍스트는 UTF-8 경계에서 각각 최대 8,192바이트로 제한하며 원본 값은 유지한다.

일반 활성화는 선택 가능한 경우 선택 상태를 전환하고 `selected_changed` 다음에
`clicked`를 호출한다. 첫 콜백에서 카드가 제거되거나 다시 설정되면 오래된 클릭은
전달하지 않는다. 메뉴와 액션은 별도 콜백으로 전달하며 메인 클릭을 발생시키지 않는다.
입력은 생성한 스레드에서만 허용하고 잘못된 설정이나 아이콘은 마지막 정상 상태를
유지한다. 외부 상태의 변경은 되돌리지 않는다.

사용자 지정 미리보기·아이콘 컴포넌트, 모든 카드 세부 표현과 실제 운영체제의
렌더링·접근성 검증은 아직 필요하다. `LVUI.OwnedCard.Model`, `Host`, `Codegen`,
`Generated`는 유효성, 좁은 영역의 배치, 상태와 콜백, 오류 복구와 수명을 창을 열지
않는 환경에서 검사한다.
