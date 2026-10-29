# LVUI 네이티브 머티리얼

기본 C++ AOT 경로는 `MaterialSurface`, `PanelMaterial`, `WindowMaterial`을 LVUI가
직접 작성한 `Material` 모델과 `NativeMaterial` 어댑터로 생성한다. LVRS의 타입이나
팩터리를 호출하지 않는다. 앱은 iiGUI의 범용 레이아웃·그리기·프레임 서비스를
공급하며, LVUI 모델 자체는 표준 C++만 사용한다. 재질 안의 자식 View는 같은 범위와
재질의 중첩 레이아웃에 남는다.

```lvui
import vm MaterialBackend
Window {
 PanelMaterial(width: 380, height: 204, density: .Glass25,
               primaryColor: vm.accent, backdrop: .PreviousLayers) {
  Label(text: "Native material")
 }
}
```

## 값과 렌더링 계약

`MaterialConfiguration`의 밀도별 기본값을 독립적으로 계산한다. 패널의 기본 크기는
380×204, 창은 500×272 논리 px이다. 유리 밀도는 25% 채색과 16px 블러,
밀집 밀도는 75% 채색과 64px 블러를 사용한다. 창의 기본 채색은 별도로 50%이다.
개별 값은 명시적으로 덮어쓸 수 있고 밀도가 반응형으로 바뀌면 생략한 값은 새 밀도에
따라 다시 계산된다. 색상과 불투명도, 테두리, 안쪽 하이라이트, 그림자와 블러 및
`.None`·`.Content`·`.PreviousLayers` 배경 캡처를 전달한다. 그리기 값의 색상은
호스트가 요구하는 선형 sRGB로 변환한다.

LVUI는 크기에 따른 여덟 타원형 방사 그레디언트의 위치와 중첩 강도를 계산한다.
강조색의 알파는 각 그레디언트가 겹치기 전에 적용한다. 그 결과는 불변 텍스처로
캐시하여 같은 크기와 강도·강조색 알파에서는 다시 만들지 않는다.
고해상도 화면에서는 텍스처 크기를 제한하고 논리 좌표에 맞게 샘플링한다.
기본 강조색은 LVUI 디자인 토큰에서 구성하며 `primaryColor`로 교체할 수 있다.
색상·채색 전환은 LVUI `Motion`과 호스트가 주입하는 프레임 시각을 사용한다.
축소 모션과 `motionEnabled: false`는 즉시 변경한다. 프레임 서비스를 받을 수 없으면
목표 색상으로 즉시 정착한다.

지원 입력은 `density`, `primaryColor`, `color`, `borderColor`, `backdropBaseColor`,
`tintOpacity`, `blurRadius`, `radius`, `innerHighlightOpacity`, `innerHighlightOffsetY`,
`borderWidth`, `intenseOpacity`, `faintOpacity`, `shadowRadius`, `shadowOffsetY`,
`shadowOpacity`, `blurEnabled`, `motionEnabled`, `enabled`, `backdrop`, `width`,
`height`, `motion`, `outlinePath` 및 공통 `visible`·`opacity`·`flex`이다. 밀도와 배경은 각각
`lvui::MaterialDensity`, `lvui::MaterialBackdrop` 값으로도 반응형 주입할 수 있다.
범위를 벗어난 반응형 값은 기존 유효 설정을 보존하고 바인딩 오류로 보고한다.
블러와 그림자 반경은 호스트 계약에 맞게 0~64 논리 px이다.

`outlinePath`는 재질의 로컬 좌표로 된 SVG 경로 데이터이다. LVUI가 명령과 좌표를
검증하고, 범용 호스트의 메모리 전용 SVG 래스터화로 불변 마스크와 테두리를 만든다.
같은 윤곽을 배경·안쪽 하이라이트·그림자에 사용하며, 값과 크기가 바뀔 때 갱신한다.
경로에는 XML이나 외부 리소스를 포함할 수 없다.
재질 헤더와 이를 사용하는 툴팁 표시 접점은 iOS·웹 대상 C++ 컴파일 검사로도 확인한다.

아직 독립 경로는 임의의 `backdropSource`·`backdropBackground` 및 창별 강조색 자동 상속을 지원하지 않는다.
지원하지 않는 입력은 오류로 거절하며, 강조색은 앱이 명시적으로 공급할 수 있다.
실제 GPU 픽셀·운영체제별 효과 및 기존 디자인과의 시각 동등성은 검증되지 않았다.

## 레거시 경로

전환 전 `FRAMEWORK LVRS`는 계속 기존 LVRS 팩터리와 옵션을 생성한다. 이 선택 경로는
아직 LVRS에 의존하고 `outlinePath`와 기존 `MotionPreferences` 타입을 받는다.
기본 독립 경로와 레거시 경로의 지원 범위를 혼동하지 않는다. LVRS 경로 자체를
제거하는 작업은 다른 컴포넌트의 독립 이식과 함께 남아 있다.
