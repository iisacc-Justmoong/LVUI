# LVUI 독립 이미지

기본 네이티브 `Image`는 LVUI의 `Image` C++ 모델로 유효성, 크기, 원형 영역,
여백, 비율과 중앙 자르기를 계산한다. `LVUI/Image.h`는 표준 C++와 LVUI 색상 값만
사용하므로 렌더러나 LVRS를 요구하지 않는다. 앱이 선택한 iiGUI 연결은
`LVUI/NativeImage.h`가 범용 컨테이너의 그리기와 접근성 접점에 그림 명령을
전달한다. iiGUI의 고수준 이미지 위젯과 이미지 옵션 구현은 호출하지 않는다.

```lvui
import vm ImageBackend
Window {
    VStack {
        Image(id: "preview", source: vm.pixels, width: 200, height: 100,
              fit: .Cover, circular: true, padding: 10,
              accessibilityName: "Preview", allowEmpty: true)
    }
}
```

`source`는 앱이 준비한 `shared_ptr<const ii::gui::RasterImage>`이며 비어 있을 수
있다. 파일·URL·SVG의 로딩, 변환과 캐시 정책은 앱이 맡는다. `.Contain`은 잘림
없이 중앙에 맞추고, `.Cover`는 정규화한 원본 영역을 중앙에서 자른다. `.Stretch`는
대상 전체에 맞춘다. `fit`을 생략하면 `preserveAspect`가 참일 때 contain, 그 외에는
stretch이다. 원형 모양은 할당된 영역의 중앙 정사각형을 사용한다. 배경과 테두리는
명시된 sRGB 색상에서 호스트의 선형 색 공간으로 변환된다.

지원 입력은 `source`, `width`, `height`, `padding`, `cornerRadius`, `borderWidth`,
`backgroundColor`, `borderColor`, `preserveAspect`, `fit`, `circular`, `allowEmpty`,
`accessibilityName`이다. `fit`의 C++ 반응형 값은 LVUI 또는 iiGUI의 대응 enum을
받는다. 설정된 크기는 양수, 여백은 크기의 절반 이하여야 하며 색상은 유한한 0–1
채널이어야 한다. 접근성 이름은 NUL이 없는 유효 UTF-8이며 최대 65,536바이트이다.
`allowEmpty: false`에서는 원본이 없는 후보를 거부한다. 여러 반응형 입력은 하나의
후보로 계산하며, 유효하지 않은 후보는 이전 이미지와 설정을 유지한다. 정상 입력으로
다시 갱신하면 복구한다. 호스트의 별도 오류나 위젯 종료가 일어난 경우는 일반적인
후보 검증 실패와 구분된다.

이미지 갱신은 소유 스레드에서 수행한다. 배치 또는 접근성 알림이 갱신 중 다시
이미지 설정을 바꾸면 나중 설정이 유지되며, 이전 호출이 그리기를 덮어쓰지 않는다.
호스트 알림이 예외를 던지면 원인이 보존된 오류를 반환한다. 이미 외부에 전달된
알림의 부수 효과는 되돌리지 않는다.

이미지 역할과 이름은 호스트의 범용 접근성 의미 정보로 전달하므로 해당 접점을
제공하는 iiGUI 버전이 필요하다. 컴파일 검사와 창을 열지 않는 호스트 검사는 픽셀
핸들, 도형, 갱신과 수명을 확인한다. 실제 OS 창, GPU 표시 및 스크린 리더 실행을
증명하지는 않는다. 기존 `FRAMEWORK LVRS` 경로와 다른 컴포넌트는 여전히 전환
대상이며 전체 독립 범위는 [독립 구현 현황](LVUIIndependence.md)을 따른다.
