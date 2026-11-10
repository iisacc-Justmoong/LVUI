# LVUI 독립 이미지

기본 네이티브 `Image`는 `iiGUI/ImageModel.h`의 모델과
`iiGUI/NativeImageView.h`의 위젯이 유효성, 배치, 접근성과 그리기를 소유한다.
LVUI는 선언된 값과 이미지 공급자 바인딩을 iiGUI에 전달한다. 위젯은 브라우저나
운영체제 기본 컨트롤을 사용하지 않으며, iiGUI와 iiSDKCore로 독립 사용 가능하다.

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

`source`는 앱이 준비한 `shared_ptr<const ii::gui::RasterImage>` 또는 파일·URL
문자열이며 비어 있을 수 있다. 문자열은 화면의 이미지 서비스 또는 개별
`imageLoader`가 해석한다. 기본 iiSDK 로더의 수명, 데이터·원격 요청, 캐시와 제한은
[이미지 로딩](NativeImageLoading.md)을 따른다. `.Contain`은 잘림
없이 중앙에 맞추고, `.Cover`는 정규화한 원본 영역을 중앙에서 자른다. `.Stretch`는
대상 전체에 맞춘다. `fit`을 생략하면 `preserveAspect`가 참일 때 contain, 그 외에는
stretch이다. 원형 모양은 할당된 영역의 중앙 정사각형을 사용한다. 배경과 테두리는
명시된 sRGB 색상에서 호스트의 선형 색 공간으로 변환된다.

C++ `Image::geometry`의 선택적 세 번째 입력과 `ii::gui::ImageInput.relative_padding`은
실제 할당 영역의 짧은 변에 대한 여백 비율을 지정한다. 원형이면 중앙 원의 지름을
기준으로 하며, 이 비율은 해당 그리기에서 절대 `padding`을 대체한다. 유한한
0–0.5만 허용하며 잘못된 값은 기존 네이티브 이미지 설정을 바꾸지 않는다. 이 입력은
현재 C++ 어댑터용이며 선언형 `Image`의 별도 속성은 아니다.

## 배치·배율 기반 래스터 공급

네이티브 C++ `ImageInput.raster_source`는 고정 래스터 대신 선택적인 래스터
공급자를 받는다. 공급자는 실제 할당 크기와 호스트 배율을 받아 그리기에 사용할 불변
래스터 또는 오류를 반환한다. 고정 래스터와 공급자를 동시에 지정한 후보는
거부한다. 배치나 배율이 바뀐 다음 그리기에는 새로운 요청을 전달하고, 캐시와
원격 로딩 정책은 공급자가 소유한다. 공급자가 빈 래스터를 반환할 때는 기존
`allowEmpty` 계약을 적용하며 오류는 그리기 오류로 전달한다. 콜백은 소유
스레드에서 호출되고 사용자 위젯 수명이나 접근성을 교체하지 않는다. 기존
고정 이미지의 자르기와 반응형 설정 계약은 유지한다. Avatar 기본 심벌은 이
경로를 사용하며 선언형 입력과 Python API의 새 속성을 뜻하지 않는다.
`ImageInput`의 선택적 `tint`는 마스크 알파에 색상의 알파를 곱하고 encoded sRGB
색상을 선형 iiGUI 색상으로 전달한다. 범위 밖이나 유한하지 않은 채널은 거부하며
고정 사진은 색상 지정이 없으면 원본 그대로 그린다. 이 입력은 네이티브 어댑터의
공통 기능이며 선언형 Image에 새로운 속성을 추가하지 않는다. `ImageInput`의
필드 구성이 변경되었으므로 C++ 소비자는 새 헤더로 다시 빌드해야 한다. 공급자나
외부 로더가 발생시킨 부수 효과는 되돌리지 않는다.

지원 입력은 `source`, `imageLoader`, `width`, `height`, `padding`, `cornerRadius`, `borderWidth`,
`backgroundColor`, `borderColor`, `preserveAspect`, `fit`, `circular`, `allowEmpty`,
`accessibilityName`이다. `fit`의 C++ 반응형 값은 `ii::gui::ImageFit`을 받는다. 기존 `lvui::ImageFit`,
`lvui::ImageConfiguration`, `lvui::native::NativeImage` C++ 사용자는 iiGUI 타입과
헤더로 이전해야 한다. 설정된 크기는 양수, 여백은 크기의 절반 이하여야 하며 색상은 유한한 0–1
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
증명하지는 않는다. 기존 `FRAMEWORK LVRS` 생성 경로는 제거되었으며 다른
컴포넌트의 이전 범위는 [독립 구현 현황](LVUIIndependence.md)을 따른다.
