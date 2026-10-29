# LVUI 색상 표면

`LVUI/ColorSurface.h`는 색상 표면의 종류, 정규화된 입력 좌표, 픽셀과 선택점으로
구성된 일관된 프레임, 교체 가능한 `ColorSurfaceSource` 인터페이스를 정의한다.
프레임 픽셀은 위쪽 행부터 저장된 비연관 RGBA8 encoded sRGB이고, 한 프레임의
색상과 선택점은 같은 리비전에 속한다. 공급자는 오래된 리비전의 입력을 거부해야 한다.
폭과 높이는 각각 12~1024 논리 단위, 래스터 축은 각각 최대 1024 픽셀이다.

선택적으로 빌드하는 `LVUI::Color`는 앱의 `ColorEditing`을 표면 공급자로 감싸는
`ColorSelectionSurface`를 제공한다. LVUI가 편집 리비전, 세션과 취소를 관리하고,
독립 iiSDK 모듈인 iiColorManager가 색 공간 계산과 래스터 샘플을 제공한다.
사용자는 `ColorSurfaceSource`를 직접 구현해 색상 계산과 저장 방식을 교체할 수 있다.
기본 LVUI 빌드는 컴파일된 iiColorManager 라이브러리를 요구하지 않는다.

`ColorSelectionSurface`는 색상환, 색조·채도, 채도·명도, 스펙트럼, 색조 및 명도
레일을 제공한다. 동일한 표면 공급자를 공유하면 검정색의 숨은 색조를 유지하며,
해당 공급자가 관찰한 편집 세션의 취소는 색상과 숨은 색조를 복원한다. 공급자가
관찰하지 못한 여러 세션이 이미 종료되었다면 이전 숨은 색조는 다시 추정하지 않는다.
외부 색상 변경에 뒤늦은 포인터 이벤트가 도착하면 이전 값을 덮지 않는다.
드래그 중 표면 종류나 공급자가 바뀌면 기존 제스처는 새 대상에 이어지지 않는다.

`ColorChannels.h`의 교체 가능한 채널 공급자는 HSB, RGB, CMYK, 알파와 회색 채널의
값·범위·일곱 색상 정지를 하나의 리비전으로 제공한다. `ColorSelectionSurface`는 같은
iiColorManager 어댑터로 이 인터페이스도 구현한다. 채널 편집은 현재 리비전을 확인하고
동일한 선택 상태와 취소 기준을 사용한다. 검정에서 색조를 바꾸는 등 RGB가 유지되는
내부 표현 변경도 리비전을 발행한다. `ColorChannels`는 범위와 색상 유효성을 검증한다.
`ColorChannelSurface`는 하나의 채널과 일곱 정지를 가로 래스터로 바꾸어 같은 표면
입력·그리기 경로에서 사용할 수 있게 한다. 래스터를 만드는 동안 색상이 바뀌거나
오래된 리비전의 입력이 오면 후보를 거부한다.

`NativeColorSurface`는 iiGUI의 범용 동작 표면에 이미지와 선택 표시를 그리며 포인터,
키보드 및 1차원 표면의 접근성 증감 입력을 모델에 연결한다. C++에서 공급자나 외부
리비전이 바뀌면 앱이 `set_options()`로 갱신해야 한다. 접근성 이름과 현재 RGBA 값을
전달하지만 2차원 색상 선택을 위한 플랫폼 보조 기술 의미 및 실제 장치 동작은 아직
검증되지 않았다.
래스터는 설정한 논리 크기로 만들며 장치 배율에 따른 재생성은 아직 지원하지 않는다.
이 표면과 채널을 조합한 [독립 색상 피커](NativeColorPicker.md)도 제공한다.

기본 선언형 `ColorPickerSurface`와 `ColorField`는 `model`에
`std::shared_ptr<lvui::ColorSurfaceSource>`를 받고 `modelRevision`을 통해 여러 표면과
버튼을 갱신한다. `field`는 `.Wheel`, `.HueSaturation`, `.SaturationBrightness`,
`.Spectrum`, `.HueRail`, `.BrightnessRail` 또는 관측 가능한 `ColorSurfaceKind`다.
`width`, `height`, `markerDiameter`, `enabled`, `accessibilityName`과
`onColorEdited`도 지정할 수 있다. 콜백은 `ii::gui::WidgetColor`를 받으며 앱에서
모델 리비전을 발행해야 다른 표면이 편집을 관측한다.

```text
import vm ColorBackend
Window {
    HStack {
        ColorPickerSurface(model: vm.surface, modelRevision: vm.revision,
                           field: .Wheel, onColorEdited: vm.edited)
        ColorField(model: vm.surface, modelRevision: vm.revision,
                   field: .HueRail, onColorEdited: vm.edited)
    }
}
```

빌드할 때 `-DLVUI_BUILD_COLOR_ADAPTER=ON`을 지정하고 설치 패키지에서
`find_package(LVUI CONFIG REQUIRED COMPONENTS Color)`와 `LVUI::Color`를 사용한다.
표면 인터페이스와 교체 공급자만 쓰는 앱은 이 선택적 구성 요소가 필요하지 않다.
