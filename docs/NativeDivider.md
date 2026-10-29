# LVUI 독립 구분선

기본 `MenuDivider`와 `ContextMenuDivider`는 `LVUI/Divider.h`의 표준 C++ 모델이
방향, 자연 크기, 선의 배치와 색상을 결정한다. 앱의 `LVUI/NativeDivider.h`
어댑터는 범용 비대화형 표면과 그리기 경계에 결과를 전달한다. 이 경로는
LVRS 구현, Qt 또는 QML 런타임을 필요로 하지 않는다.

```lvui
import vm EditorBackend
Window {
    VStack {
        MenuDivider(axis: vm.axis, lineLength: vm.length)
        ContextMenuDivider()
    }
}
```

`axis`는 `.horizontal`, `.vertical`, 대소문자를 구분하지 않는 문자열 또는
반응형 `lvui::DividerAxis`·문자열을 받는다. `lineLength`, `thickness`,
`crossPadding`, `linePadding`, `dividerColor`도 상수 또는 반응형 입력이다.
기본 일반 선은 220 × 3, 맥락 메뉴 선은 145 × 3이며 후자는 선 양끝에 4의
여백이 있다. 기본 색상은 LVUI가 보유한 디자인 토큰 스냅샷에서 선택한다.
색상 입력은 encoded sRGB이며 그리기 경계에서 선형 RGB로 변환한다.

자연 크기의 교차축은 선 두께와 양쪽 교차 여백의 합이다. 실제 할당이 더 작으면
선의 두께와 끝 여백을 사용 가능한 영역으로 제한한다. 입력 후보 전체를 검증한 뒤
기존 표면에 반영하며 잘못된 축, 음수·비유한 크기, 과도한 자연 크기나 잘못된
색상은 이전 상태를 유지한다. 이후 유효한 후보가 들어오면 반응형 바인딩은 복구한다.
구분선은 활성화 또는 입력 대상을 만들지 않는다.

독립 `Menu`와 `ContextMenu`의 직접 자식으로 등록하면 부모의 항목 폭에 맞춰
그려지며 구분선의 높이와 표시 여부가 메뉴 크기에 반영된다. C++에서 메뉴에 등록한
구분선의 표시 여부는 표면 핸들 대신 `NativeDivider::set_visible`로 변경해야 메뉴가
변경을 관찰한다. 호스트 모델 검사는 창이나 GPU를 실행한 결과가 아니다.
