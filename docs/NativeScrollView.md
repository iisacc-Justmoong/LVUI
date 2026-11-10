# LVUI ScrollView

`ScrollView`는 별도 색상이나 제품 스타일을 소유하지 않는 iiGUI 스크롤 뷰포트이다.
정확히 하나의 직접 자식 뷰를 유지한다. 여러 컨트롤이 필요하면 스택으로 묶는다.
뷰포트에서 벗어난 내용은 잘리며, 휠·트랙패드 입력의 중첩 전파와 포커스된 자식
드러내기는 iiGUI 호스트가 담당한다.

```lvui
ScrollView(id: "parameters", width: vm.panel_width, height: vm.available_height) {
    VStack(spacing: Metric.space12) {
        Slider(size: .mini, value: vm.size, onMoved: vm.set_size)
        Slider(size: .mini, value: vm.opacity, onMoved: vm.set_opacity)
    }
}
```

`width`와 `height`는 0–8192의 유한한 논리 픽셀이며 0은 호스트의 자연 크기
계산을 따른다. 부모의 늘림 정렬은 지정된 자연 크기보다 큰 영역을 할당할 수
있다. `enabled`, `showIndicator`, `horizontal`은 각각 스크롤 허용, 표시기와 가로
스크롤을 선택하며 기본값은 `true`, `true`, `false`이다. 이 다섯 입력은 상수 또는
반응형 값이 될 수 있다. 범위를 벗어난 반응형 크기는 오류로 보고하고 이전 유효
설정을 유지한다. 일반적인 `visible`, `opacity`, `flex`도 사용할 수 있다.
`enabled`는 스크롤 조작을 제어하며 내부 컨트롤을 비활성화하지 않는다.
배치는 `VStack`의 입력으로 표현한다. `Label(style: .body)`의 열거형 `style`은
공통 텍스트 스타일 선택이다.

C++23 AOT와 Python 컴파일 브리지는 같은 언어 계약을 사용한다. iiGUI의
`WidgetBuilder::scroll_view`와 `Widget::set_scroll_options`가 교체 가능한 네이티브
호스트 경계이다. 헤드리스 테스트는 내용 크기와 클리핑, 휠 입력, 마지막 자식의
클릭 및 포커스 노출, 유효하지 않은 반응형 크기의 거절과 회복, 뷰 수명을 확인한다.
실제 운영체제 창·입력 장치·접근성 및 GPU 픽셀 결과를 이 검사로 주장하지 않는다.
