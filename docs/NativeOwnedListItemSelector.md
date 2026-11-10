# LVUI 독립 목록 선택기

iiGUI의 `ListItemSelectorModel.h`와 `NativeListItemSelectorView.h`가 선택 상태와
콤보 상자·메뉴 조합을 소유한다. LVUI는 [선언값과 바인딩](ListItemSelector.md)을
제공하며 생성된 앱이 iiGUI 구현을 명시적으로 조합한다. 옵션은 텍스트와 활성 여부를 가지며 선택 인덱스와 화면
크기, 표시 여부, 접근성 이름을 C++ 입력으로 제공한다. 위·아래 키는 비활성 옵션을
건너뛰고 목록 끝에서 멈춘다. 버튼을 누르면 같은 위젯 세대에 속한 메뉴가 열리고,
사용 가능한 옵션을 선택하면 메뉴를 닫은 뒤 변경 이벤트를 전달한다.

옵션 목록이나 크기가 바뀌면 새 버튼과 메뉴를 준비해 기존 세대를 교체한다. 잘못된
옵션은 거부하며 동일한 값을 반응형으로 되돌려 받으면 중복 이벤트를 만들지 않는다.
선택 콜백 안에서 옵션 목록을 변경하면 콜백이 반환된 뒤 안전하게 화면을 교체한다.
선택기 및 메뉴를 교체하거나 부모가 제거된 뒤에는 이전 위젯 핸들을 사용할 수 없다.
`ListItemSelectorInput::accent`는 선택적으로 해결된 `WindowAccentPalette`를 받는다.
선택기 포커스와 메뉴 항목의 선택·누름·포커스에 적용하며, 색상만 변경하면 열린
메뉴와 기존 항목, 선택 및 포커스를 유지한다. 유효하지 않은 색상은 거부하고 기존
입력을 보존한다. 미지정 시 기본 팔레트를 사용한다. 복합 항목 안의 선택기는 부모가
팔레트를 전달하며, 독립 C++ 사용자는 직접 지정할 수 있다.
수량·설정·작업·폼 복합 항목에서 선언의 기본 문구는 각각 `Format`, `Automatic`, `Priority`, `Category`이며 단위 선택기는
`Units`를 사용한다. 문자열 배열을 이 문구와 함께 구성할 수 있고, 앱이 직접 제공한
설정의 자리표시 문구는 덮어쓰지 않는다. iiGUI 선택 모델의 `validate_options()`는
옵션 목록의 UTF-8·개수·바이트 제한을 사본 생성 없이 검증한다.
긴 메뉴는 키보드 포커스와 포인터 스크롤에 따라 표시 영역을 이동한다. 호스트 접근성
역할과 실제 플랫폼 표시 동작은 별도의 기기 검증이 필요하다.

`LVUI.OwnedListItem.SelectorHost`는 창을 열지 않는 위젯 트리에서 키보드, 메뉴 선택,
옵션 변경, 비활성 상태, 소유 스레드와 수명을 검사한다. 선언형 목록 항목에서도
`.Select`로 사용할 수 있다.

## 독립 선언

`.lvui`에서 목록 항목의 내부 구현을 직접 참조하지 않고 선택기를 View로 작성할 수 있다.
정적 문자열 배열 또는 C++에서 제공한 `std::vector<std::string>`,
`std::vector<lvui::ListItemOption>`, `lvui::ListItemSelectorConfiguration`을
`options`로 전달한다. 설정 객체의 옵션·선택·문구·활성 상태를 보존하고,
명시적으로 지정한 `currentIndex`, `placeholder`, `enabled`가 해당 필드를 대체한다.
선택 인덱스는 0부터 시작하며 빈 목록의 인덱스는 0이다. 항목은 400개까지 허용한다.

```lvui
import vm PublishingModel
ListItemSelector(options: vm.options, currentIndex: vm.index,
    enabled: vm.enabled, onCurrentIndexChanged: vm.changed)
```

`width`, `height`, `visible`, `accessibilityName`도 선언할 수 있으며 반응형 입력은
하나의 설정으로 평가하여 적용한다. 선택 이벤트는 `int` 인덱스를 전달한다. 잘못된
갱신은 오류로 기록하고 마지막 정상 선택기를 유지한다. `LVUI.NativeView.selector`와
`LVUI.NativeView.StandaloneSelector`는 C++ 생성 및 위젯 트리의 갱신·퇴역을 검사한다.

게시 전 설정을 준비할 수 있으며 퇴역 시 콜백과 하위 위젯 참조를 해제한다. 콜백이 퇴역 후 예외를 던져도 오류 저장소가 그 자원을 붙잡지 않는다. macOS arm64에서 독립 소비자와 생성·복합 목록 경로를 창 없이 검사했다. 실제 운영체제 표시와 GPU는 검사하지 않았다.
