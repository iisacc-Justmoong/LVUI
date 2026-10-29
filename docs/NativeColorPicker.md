# LVUI 색상 피커 편집 모델

`LVUI/ColorPicker.h`는 Qt, QML, LVRS 또는 색 공간 라이브러리에 의존하지 않고
피커의 설정과 편집 세션을 소유한다. 실제 색상 상태는 앱이 주입한 `ColorEditing`
인터페이스를 통해 사용하므로 [기본 ColorSelection](ColorSelection.md)과 다른 편집
모델을 교체할 수 있다. 휠·HS·SV·그레이스케일·RGB·CMYK 모드, 폭 320~1024,
패딩 0~64와 최소 콘텐츠 폭 228을 검증한다.

`begin()`은 편집 토큰을 소유하고 `set_color()` 또는 `set_hex()`로 수정한다.
HEX는 선택적 `#`과 6자리 RRGGBB 또는 8자리 RRGGBBAA를 받으며, 6자리는 현재 알파를
유지한다. HEX가 계산된 리비전이 바뀌면 수정은 거부되어 새 외부 색상을 덮지 않는다.
`accept()`는 실제 완료된 색상·리비전을 원자적으로 받아 최대 10개의 로컬 최근 색상에
중복 없이 기록한다. 자동 기록을 끄거나 초기 목록을 앱이 제공할 수 있으며 기본 목록은
비어 있다. `cancel()`은 같은 세션의 기준만 복원한다.

편집 중에는 다른 편집 모델로 교체할 수 없다. 소유 피커가 해제되면 미완료 세션을
취소하려고 시도하며, 이미 외부 색상이 대체하여 토큰이 만료됐다면 그 색상을 보존한다.
피커 모델 자체는 단일 소유자가 사용하고, 기본 `ColorSelection`은 동시 접근을
직렬화한다. 교체 구현은 동일한 토큰·리비전·원자적 완료 계약을 제공해야 한다.

`ColorPickerSession`은 편집 모델과 교체 가능한 표면·채널 공급자를 연결한다.
세 공급자는 같은 색상과 리비전을 설명해야 하며, 다르면 입력을 거부한다. 첫
제스처는 관찰된 리비전에서 원자적으로 세션을 열고, 이후 표면과 채널 입력은
같은 취소 기준을 공유한다. 오래된 입력이나 세션 시작 중 외부 변경은 거부한다.
공급자 수명을 앱이 관리하고 앱이 외부 리비전 갱신을 컨트롤에 알려야 한다.

`ColorPickerLayout`은 여섯 형태의 채널 순서, 평면·레일, 견본, 값, 최근 색상과
액션의 논리 좌표를 계산한다. 폭 320과 패딩 12일 때 각 형태의 높이는 휠 461,
HS 및 SV 437, 회색 361, RGB 413, CMYK 439이다. 최근 색상이나 액션을 숨기면
각각 32 또는 34만큼 줄고 평면은 콘텐츠 폭에 맞춰 커진다.

`NativeColorPickerValues`는 이전·현재 색상 비교 견본과 HEX·알파 입력 행을
제공한다. Enter 또는 포커스 이탈 시 수정한 값만 확정하며, 잘못된 입력은 복원한다.
입력 중 다른 편집이나 외부 변경이 발생하면 오래된 값을 적용하지 않는다. 여섯 자리
HEX는 알파를 유지하고 여덟 자리는 알파도 변경한다. 앱이 외부 리비전을 전달해
표시를 갱신하며 두 필드와 견본은 동일한 편집 세션을 사용한다.

`NativeColorPicker`는 이 모델과 [색상 표면·채널](NativeColorSurface.md), 값 입력,
최근 색상, 적용·취소와 스포이트 버튼을 직접 조합한다. 부모는 배치와 열기·닫기를
담당하며 피커는 스스로 닫히지 않는다. 적용과 취소는 포커스된 보류 입력을 먼저
확정한다. 취소와 Escape는 현재 세션의 기준을 복원하고 색상이 바뀌었다면 편집
콜백도 전달한다. 스포이트는 앱에 요청만 전달하며 실제 화면이나 문서의 색상 채취는
앱이 수행한다. 폐기된 피커는 진행 중인 자신의 세션을 취소한다. 게시되지 못한 후보는
기존 세션과 설정을 건드리지 않는다. 한 피커가 하나의 `ColorPickerSession`을 소유하며,
서로 다른 피커가 같은 편집 모델을 사용하면 한 번에 하나만 편집할 수 있다.

기본 선언형 `ColorPicker`는 이 독립 조합을 생성한다. `model`을 생략하면 인스턴스별
`ColorSelection`과 iiColorManager 기반 `ColorSelectionSurface`를 만들므로 빌드에서
`LVUI_BUILD_COLOR_ADAPTER=ON`, 소비 프로젝트에서 `LVUI::Color` 링크가 필요하다.
`model`에 `std::shared_ptr<lvui::ColorPickerSession>`을 주입하면 그 구현을 사용하며
앱은 외부 변경을 관측 가능한 `modelRevision`으로 발행한다. 타입은 `.Wheel`,
`.HueSaturation`, `.SaturationBrightness`, `.Grayscale`, `.RGB`, `.CMYK` 또는
관측 가능한 `ColorPickerMode`다. 폭·패딩·활성화, 최근 색상과 액션 표시, 액션 텍스트,
접근성 이름, 자동 최근 색상 기록은 반응형으로 변경할 수 있다. `recentColors`는
정적 색상/HEX 목록 또는 관측 가능한 `std::vector<lvui::StyleColor>`를 받는다. 정적
목록은 처음 10개를 사용하고 동적 목록은 최대 10개를 요구한다. 목록을 생략하면
확정한 색상의 로컬 기록을 반응형 옵션 갱신 중에도 유지한다. 기본 기록은 비어 있다.
`onColorEdited`와 `onAccepted`는 `ii::gui::WidgetColor`를, `onCanceled`와
`onEyedropperRequested`는 인자를 받지 않는다.

```text
import vm ColorBackend
Window {
    ColorPicker(model: vm.session, modelRevision: vm.revision, type: .RGB,
                onColorEdited: vm.edited, onAccepted: vm.accepted,
                onCanceled: vm.canceled, onEyedropperRequested: vm.eyedropper)
}
```

헤드리스 테스트는 여섯 배치, 표면·채널·값 입력, 적용·취소, 기존 편집을 보존하는
생성 실패, 동적 옵션과 폐기를 확인한다. iOS 15 대상 헤더 구문은 별도로 확인했으며,
실제 플랫폼 창·입력기·보조 기술 및 시각적 동등성은 아직 검증하지 않았다.
