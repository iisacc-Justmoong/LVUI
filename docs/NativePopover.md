# LVUI 독립 팝오버

기본 선언형 `Popover`는 `LVUI/Popover.h`의 값 모델과
`LVUI/NativePopover.h`의 앱 호스트 어댑터로 구성한다. LVUI가 유효한 디자인 값과
반응형 업데이트를 소유하고 iiGUI의 범용 팝업 경계가 배치, 표면, 입력과 포커스를
전달한다. LVRS, Qt 또는 QML 구현을 호출하지 않는다.

```lvui
import vm EditorBackend
Window {
    Popover(id: "details", width: vm.width, surfaceOpacity: 0.25,
            onOpened: vm.opened, onClosed: vm.closed) {
        VStack { Label(text: vm.description) }
    }
}
```

`width`와 `height`의 0은 콘텐츠에 따른 자동 크기이다. 기본 여백은 12, 화면 가장자리
여백은 8, 반경은 12, 표면 채색 강도는 0.25이다. `surfaceColor`, `modal`, `dim`,
`focus`, `clip`, `closeOnEscape`, `closeOnPressOutside`, `closeOnContextRequest`,
`accessibilityName`과 열림·닫힘·취소 콜백을 설정할 수 있다. 문맥 요청 정책을
생략하면 일반 외부 클릭 정책을 따른다. 콘텐츠는 하나의 직접 View이며 여러 요소는
스택에 담는다. 별도 팝업은 중첩할 수 있다.

크기·색상·정책 입력은 반응형으로 변경할 수 있으며 잘못된 후보는 기존 상태와
콘텐츠를 유지하고 바인딩 오류를 전달한다. 생성된 위젯 핸들의 `open_at`, `open_for`,
`close_popup`으로 표시 수명을 제어한다. 닫힘 콜백이 새 팝오버를 열었다면 이전
취소 이벤트는 새 열린 상태에 전달하지 않는다. 팝오버가 제거되면 핸들도 만료된다.

이 경로는 범용 호스트의 팝업 표면에 디자인 값을 전달한다. 흐림·그림자·전환 모션의
세부 디자인, 플랫폼별 실제 표시와 보조 기술 동작은 별도 구현 또는 검증이 필요하다.
헤드리스 상태 검사는 창이나 GPU를 실행한 결과가 아니다.
