# 네이티브 레이블 자간과 반응형 크기

`Label`의 `letterSpacing`은 iiGUI 자간을 논리 픽셀로 전달한다. 기본값은 0이며
유효 범위는 -256부터 256까지이다. `fontSize`, `lineHeight`, `letterSpacing`은
상수 또는 관찰 가능한 뷰모델 값으로 지정할 수 있다.

`lineHeight`를 생략하고 `fontSize`를 지정하면 현재 글꼴 크기를 줄 높이로 쓴다.
반응형 입력은 `LabelOptions` 묶음으로 검증하고 변경하며, 잘못된 값은 기존
표시를 보존하는 바인딩 오류가 된다. 텍스트 측정과 플랫폼 렌더링의 제한은
iiGUI의 `docs/TextTracking.md`를 따른다.

생성 코드와 헤드리스 위젯 검사는 실제 브라우저 화면의 픽셀 또는 기기별 입력
검증과 구분한다.
