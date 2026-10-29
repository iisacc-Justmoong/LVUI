# LVUI 독립 버튼

기본 `AbstractButton`, `PushButton`, `LabelButton`과 `IconButton`은
`LVUI/Button.h`의 C++ 상태·색상·콘텐츠 배치 모델로 구현한다. 앱 측
`LVUI/NativeButton.h`는 범용 동작 표면과 글꼴 측정·그리기 접점을 사용한다.
버튼의 배경·텍스트·아이콘·포커스 표시는 LVUI가 선택하고 그린다. 호스트는
포인터 대상 지정, 키보드·포커스·접근성과 텍스트 셰이핑을 제공한다.

```lvui
import vm EditorBackend
Window {
    VStack {
        PushButton(text: vm.title, enabled: vm.ready, onClick: vm.save)
        IconButton(icon: vm.toolIcon, accessibilityName: "Select tool", onClick: vm.selectTool)
        IconButton(iconName: "generaladd", accessibilityName: "Add", onClick: vm.add)
    }
}
```

`text`, `enabled`, `tone`, `height`, `horizontalPadding`, `cornerRadius`,
`accessibilityName`, `iconMode`, `iconSize`, `icon`, `iconName`, `iconSource`,
`iconRevision`은 해당 타입의 상수 또는 반응형 입력을
받는다. 동적 tone은 `lvui::ButtonTone` 또는 호스트의 대응 enum을 받을 수 있다.
`icon`은 앱이 제공하는 `shared_ptr<const ii::gui::RasterImage>`이고 `iconName`은
[LVUI의 내장 아이콘 또는 교체 가능한 공급자](NativeIcons.md)를 조회한다. 두 입력을
동시에 사용할 수 없다. 이름을 찾지 못하거나 SVG가 유효하지 않으면 오류를 반환하고
기존 버튼을 유지한다. 아이콘 모드에서는 유효한 원본이 필요하다. 파일·URL 로딩은
이 경로에서 제공하지 않는다.

primary·default·borderless·destructive·disabled 색상과 기본 치수는 LVUI의
스타일 토큰 스냅샷으로 생성한다. 눌림은 호버보다 우선하며 disabled tone은
사용자 요청 enabled 값과 별도로 입력을 막는다. 반응형 값은 하나의 후보로
검증한 뒤 기존 동작 표면에 반영한다. 잘못된 크기·텍스트·색상·enum 또는 누락된
아이콘은 이전 버튼을 유지하고 정상 후보가 들어오면 복구한다. 텍스트는 현재
해결된 애플리케이션 글꼴을 사용하며 글꼴이 바뀌어도 버튼을 재생성하지 않는다.
할당된 너비보다 긴 문구는 사용 가능한 영역에 맞추어 줄임 표시한다. 원문과
접근성 이름은 줄이지 않는다.

버튼은 비소유 스타일 포트를 통해 부모가 임시 무테 톤을 적용하도록 허용한다.
이때 원래 반응형 톤을 보존하고 임시 설정을 해제하면 최신 원래 톤을 복원한다.
disabled 톤은 임시 스타일로 활성화되지 않는다. 폐기된 버튼은 포트를 통해
변경할 수 없다. [세그먼트 묶음](NativeSegmentedControl.md)은 이 포트를 조합한다.
버튼 모델은 필요한 경우 명시적 너비와 텍스트 세로 보정값도 보유한다.
[도움말 버튼](NativeHelpButton.md)은 이 설정을 원형 기본값에 사용한다.
앱 측 C++ 조합은 선택적 테두리 색상과 폭을 모델에 지정할 수 있다. 테두리는 버튼
내부에 그리며 포커스 표시와 독립적이다. 기본값은 테두리 없음이다.

이 경로는 앱이 `WidgetPaintState::font_family`를 제공하는 iiGUI 호스트를
연결해야 한다. 기본 LVUI 모델은 iiGUI와 LVRS를 링크하지 않는다. 현재 범위는
위 입력과 단일 클릭 콜백이며 전체 이름 자산, 메서드 목록, 원본의 모든 모션·
상속 API와 실제 플랫폼 접근성 검증은 아직 완료되지 않았다. 호스트 검사는
창을 열지 않으며 GPU·기기 실행 증거로 취급하지 않는다.
