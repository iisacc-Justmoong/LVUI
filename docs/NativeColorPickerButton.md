# LVUI 독립 색상 버튼

`ColorPickerButton`의 크기, 색상, 상태별 배경과 원형 배치는
`LVUI/ColorPickerButton.h`의 C++ 모델이 소유한다. `NativeColorPickerButton`은 범용
동작 표면과 메모리 이미지 디코더에 모델 결과를 전달한다. 시각 언어에 사용한 색상환
이미지만 LVUI의 `resources/images/color-picker-hue-ring.png`에 보관하며, 빌드 시
검증해 내장하므로 실행 중 레거시 모듈이나 리소스 경로를 찾지 않는다. 이 자산의
SHA-256은 `75e39c9111d53f25e87fb1bd44eb7048074721bcb22a2d337d4e11927c94c427`이다.

기본 색은 보라색, 버튼 크기는 28, 패딩과 사각 반경은 4이다. 색상환의 중앙에 현재
색상 원을 두며 비활성 상태는 전체 불투명도를 0.32로 낮춘다. `ColorSource`는 앱이
교체 가능한 색상과 리비전 공급 경계다. 같은 리비전 동안 색상은 안정적이어야 하며
읽는 도중 리비전이 반복해서 바뀌면 이전 표시를 유지하고 오류를 돌려준다.
기본 선언형 `ColorPickerButton`은 `currentColor`에 관측 가능한 색상을 전달하거나
`model`에 `std::shared_ptr<const lvui::ColorSource>`를 전달할 수 있다. 둘은 동시에
지정하지 않는다. 공급자 내부 값이 바뀌면 앱이 관측 가능한 `modelRevision`도
발행해야 다시 읽는다. 공급자 교체와 리비전은 반응형으로 연결되며 널 공급자,
잘못된 색상과 불안정한 읽기는 오류로 기록하고 마지막 정상 표시를 유지한다.
앱과 여러 컴포넌트가 색상 편집 및 취소 상태를 공유하려면 독립적인
[`ColorSelection`](ColorSelection.md)을 공급자로 사용할 수 있다.
`buttonSize`는 `.Small`·`.Medium`·`.Large`와 8~256의 수치 또는 반응형 값을 받는다.
`showHueRing`, `enabled`, `circular`, `padding`, `cornerRadius`, `borderOpacity`,
색상별 스타일, 접근성 이름과 누름·해제·취소·클릭 콜백을 설정할 수 있다.

실제 플랫폼 그리기·보조 기술 검증과 색상 전환 모션은 남아 있다. 색상 버튼은 피커나
팝오버를 자동 생성하지 않는다.
