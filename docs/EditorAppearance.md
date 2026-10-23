# 네이티브 편집기 외형 입력

버튼의 height, horizontalPadding, cornerRadius와 accessibilityName을 정적 입력으로 지정할 수 있다. icon은 LVUI 스코프가 소유한 백엔드의 공개 RasterImage 멤버를 참조한다. 파일 경로를 런타임에 평가하지 않는다. iconName의 기존 아이콘 등록 계약도 유지한다.

Slider는 thumbDiameter, trackHeight, rangeInset을 받으며 기본값은 각각 18, 6, 9이다. Hierarchy/List는 backgroundColor, selectionColor, textColor와 selectionRadius를 받는다. ColorPickerButton.circular은 원형 색상 표시를 선택한다. 생성 코드는 LVRS/iiGUI의 타입 있는 입력을 사용한다. 잘못된 크기·색상·스코프 참조는 AOT 단계에서 거부하며 editor_styles 컴파일러 검사가 이를 확인한다.

Hierarchy/List의 fontSize는 기본 13이며 Vincent 레이어 목록은 11을 사용한다. ColorPickerButton.borderOpacity는 0..1의 정적 값으로 원형 테두리의 불투명도를 지정한다. 기본 1이다. editor_styles 검사는 두 값의 C++ 전달을 함께 확인한다.

Slider.height는 네이티브 컨트롤의 실제 배치 높이를 지정한다. 기본 32이며 브러시 팝업은 12를 사용한다. 손잡이와 트랙의 높이를 함께 지정하여 그리기와 배치 영역을 일치시킨다.
