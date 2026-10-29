# 네이티브 편집기 외형 입력

기본 버튼의 height, horizontalPadding, cornerRadius, accessibilityName과 icon은
LVUI 자체 모델에 상수 또는 반응형 입력으로 전달한다. icon은 앱이 공급하는
불변 RasterImage이며 파일 경로를 런타임에 평가하지 않는다. 이름 기반 iconName은
기본 독립 경로에 아직 구현되지 않았으며 레거시 LVRS 경로와 구분한다. 자세한
계약은 [네이티브 버튼](NativeButton.md)을 따른다.

Slider는 thumbDiameter, trackHeight, rangeInset을 받으며 기본값은 각각 18, 6, 9이다. 기본 Slider는 [LVUI 독립 구현](NativeSlider.md)을 사용한다. Hierarchy/List는 backgroundColor, selectionColor, textColor와 selectionRadius를 받는다. ColorPickerButton.circular은 원형 색상 표시를 선택한다. Hierarchy/List와 ColorPickerButton은 아직 레거시 LVRS 생성 경로에 속한다. 정적 크기·색상·스코프 참조는 AOT 단계에서 검사하고 반응형 슬라이더 설정은 적용 전에 검증한다.

Hierarchy/List의 fontSize는 기본 13이며 Vincent 레이어 목록은 11을 사용한다. ColorPickerButton.borderOpacity는 0..1의 정적 값으로 원형 테두리의 불투명도를 지정한다. 기본 1이다. editor_styles 검사는 두 값의 C++ 전달을 함께 확인한다.

Slider.height는 네이티브 컨트롤의 실제 배치 높이를 지정한다. 기본 32이며 브러시 팝업은 12를 사용한다. 손잡이와 트랙의 높이를 함께 지정하여 그리기와 배치 영역을 일치시킨다.
