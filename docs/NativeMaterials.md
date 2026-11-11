# LVUI 네이티브 머티리얼

기본 C++ AOT 경로는 `MaterialSurface`, `PanelMaterial`, `WindowMaterial`을 LVUI가
직접 작성한 `Material` 모델과 `NativeMaterial` 어댑터로 생성한다. LVRS의 타입이나
팩터리를 호출하지 않는다. 앱은 iiGUI의 범용 레이아웃·그리기·프레임 서비스를
공급한다. `iiGUI/MaterialModel.h`가 재질 검증·상태·프레임 보간과 방사형 계산을
소유하며 LVUI는 Core 외 SDK 참조 없이 디자인 값과 정책을 제공한다. 재질 안의 자식 View는 같은 범위와
재질의 중첩 레이아웃에 남는다.

```lvui
import vm MaterialBackend
Window {
 PanelMaterial(width: 380, height: 204, density: .Glass25,
               primaryColor: vm.accent, backdrop: .PreviousLayers,
               backdropBackground: "background", backdropSource: vm.source) {
  Label(text: "Native material")
 }
 Label(id: "background", text: "Background")
}
```

## 값과 렌더링 계약

`MaterialConfiguration`의 밀도별 기본값을 독립적으로 계산한다. 패널의 기본 크기는
380×204, 창은 500×272 논리 px이다. 유리 밀도는 25% 채색과 16px 블러,
밀집 밀도는 75% 채색과 64px 블러를 사용한다. 창과 패널 모두 선택한 밀도의
불투명도를 사용한다. 기본 창 재질의 색상은 `Panel/window`이며 앱의 강조색과
무관하다. 창 재질에는 악센트 방사형 밑그림을 생성하지 않는다. 패널과 일반
재질은 기존처럼 창의 `accent` 또는 명시한 강조색을 사용할 수 있다.
창 재질에 전달된 `primaryColor`, `intenseOpacity`, `faintOpacity`는 창 바탕에
악센트를 더하지 않는다. 색상을 따로 지정하지 않으면 AOT 컴파일은 현재 스타일
프로필의 `Panel/window`를 창 재질 색상으로 전달한다.
개별 값은 명시적으로 덮어쓸 수 있고 밀도가 반응형으로 바뀌면 생략한 값은 새 밀도에
따라 다시 계산된다. 색상과 불투명도, 테두리, 안쪽 하이라이트, 그림자와 블러 및
`.None`·`.Content`·`.PreviousLayers` 배경 캡처를 전달한다. 그리기 값의 색상은
호스트가 요구하는 선형 sRGB로 변환한다.

배경 캡처·블러·그레디언트·윤곽·테두리·하이라이트·그림자가 없는 완전 불투명한
직사각형은 별도의 재질 텍스처 대신 같은 선형 색상의 iiGUI 패널로 전달한다. 색상이나
설정이 바뀌어 이 조건을 벗어나면 기존 재질 경로로 전환하며, 다시 조건을 만족하면
단색 패널로 돌아간다. 색상 전환 중에도 표시 중인 알파를 기준으로 선택한다. 이 경로는
그리기 명령을 줄이는 구현이며 실제 GPU의 성능 측정이나 픽셀 동일성 검증은 아니다.

iiGUI 모델은 LVUI 정책이 제공하는 여덟 타원형 방사 필드의 위치와 중첩 강도를 계산한다.
강조색의 알파는 각 그레디언트가 겹치기 전에 적용한다. 기하와 알파 기여도를
캐시하고 크기·강도·강조색이 모두 같으면 결과 텍스처를 재사용한다.
고해상도 화면에서는 텍스처 크기를 제한하고 논리 좌표에 맞게 샘플링한다.
`iiGUI/MaterialGradient.h`의 `BasicMaterialGradient`가 이 래스터와 캐시를
소유하며 일반 재질과 Popover가 함께 사용한다. LVUI의 `MaterialGradientPolicy`는
디자인 데이터와 기하 리비전을 제공하며 Core 외 SDK를 직접 참조하지 않는다.
강조색만 변하면 기하 캐시를 재사용하고 기하 리비전이 바뀌면 다시 계산한다.
단위 검사에서 1x·2x 크기와 캐시 갱신을 확인한다.
블러가 있는 재질은 반경에 따라 경계 밖에도 악센트 픽셀을 생성해 흐림 입력으로
제공한다. 여백은 반경을 올림한 값과 최소 2px을 기준으로 하고 최대 64px이며,
각 축의 밑그림이 호스트 한도 8192px을 넘지 않도록 제한한다. 블러가 없으면
여백을 만들지 않는다. 일반 재질과 팝오버가 같은 좌표·배율·캐시 계산을 사용한다.
검사는 픽셀 내용과 그리기 범위를 확인하며 실제 GPU 픽셀 동등성 검증은 아니다.
개명된 [LVUI Popover 원본](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI?node-id=997-24)의
여덟 방사형 도형은 중심 정지점에 공통 강조색을 사용하고 중간 정지점 두 개에는
별도의 고정 파랑을 사용하며 마지막 정지점은 투명한 검정이다. 일반 재질과
팝오버가 같은 다색 RGBA 밑그림을 사용한다.
정적·반응형 창 강조색은 재사용 View 안의 재질에도 적용된다. 재질의 `primaryColor`를
명시하면 창 강조색보다 우선한다. 창 변경이 호스트에서 거부되면 기존 재질 색상은
유지한다. 직접 C++로 구성한 `NativeMaterial`은 자동 상속 대상이 아니며 `primary_color`를
통해 앱이 값을 공급한다.
색상·채색 전환은 LVUI의 `Motion` 선언 값을 iiGUI 전환기에 주입하고 호스트의 프레임 시각을 사용한다.
축소 모션과 `motionEnabled: false`는 즉시 변경한다. 프레임 서비스를 받을 수 없으면
목표 색상으로 즉시 정착한다.

지원 입력은 `density`, `primaryColor`, `color`, `borderColor`, `backdropBaseColor`,
`tintOpacity`, `blurRadius`, `radius`, `innerHighlightOpacity`, `innerHighlightOffsetY`,
`borderWidth`, `intenseOpacity`, `faintOpacity`, `shadowRadius`, `shadowOffsetY`,
`shadowOpacity`, `blurEnabled`, `motionEnabled`, `enabled`, `backdrop`, `width`,
`height`, `motion`, `outlinePath`, `backdropBackground`, `backdropSource` 및 공통
`visible`·`opacity`·`flex`이다. 밀도와 배경은 각각
`lvui::MaterialDensity`, `lvui::MaterialBackdrop` 값으로도 반응형 주입할 수 있다.
범위를 벗어난 반응형 값은 기존 유효 설정을 보존하고 바인딩 오류로 보고한다.
블러와 그림자 반경은 호스트 계약에 맞게 0~64 논리 px이다.

`outlinePath`는 재질의 로컬 좌표로 된 SVG 경로 데이터이다. LVUI가 명령과 좌표를
검증하고, 범용 호스트의 메모리 전용 SVG 래스터화로 불변 마스크와 테두리를 만든다.
같은 윤곽을 배경·안쪽 하이라이트·그림자에 사용하며, 값과 크기가 바뀔 때 갱신한다.
경로에는 XML이나 외부 리소스를 포함할 수 없다.
재질 헤더와 이를 사용하는 툴팁 표시 접점은 iOS·웹 대상 C++ 컴파일 검사로도 확인한다.

`backdropBackground`와 `backdropSource`는 각각 배경과 콘텐츠를 지정한다. 문자열은
같은 생성 범위의 위젯 ID이며 대상이 재질 뒤에 선언되어도 연결된다. 다른 생성
범위의 ID나 존재하지 않는 ID는 컴파일 오류다. 스코프가 공급하는 반응형
`ii::gui::Widget` 값도 사용할 수 있으며, 기본 생성 위젯이나 빈 문자열은 소스를
비운다. 두 소스는 기본색과 선택적인 `backdrop` 캡처 위에 배경·콘텐츠 순으로
합성된다. 소스 변경 시 화면이 다시 그려지고, 소스 세대가 종료되면 같은 ID의
다음 위젯을 자동으로 가리키지 않는다. 자기 자신·조상·자식이나 다른 창 및
직·간접 순환의 새 입력은 거절하며 기존 유효 입력을 유지한다. 반응형 입력 오류는
기존 바인딩 진단으로 보고하고 이후 유효한 값으로 복구한다.

iiGUI의 [위젯 장면 계약](../../iiGUI/docs/SubtreeScenes.md)은 팝업 포털과 중첩
소스를 포함한다. Metal, Vulkan, Direct3D 11과 WebGL2 소스에는 투명 GPU 표면에 두
장면을 합성하는 경로가 있다. Direct3D와 WebGL 경로의 실제 GPU 실행은 검증하지
않았다. 운영체제 강조색의 자동 감지와 다른 독립 위젯으로의 일괄 상속은
제공하지 않는다. 이 경로의 실제 GPU 출력은 검증하지 않았다.
실제 GPU 픽셀·운영체제별 효과 및 기존 디자인과의 시각 동등성은 검증되지 않았다.

## 원본 다색 방사형 재질

원본 `Material/Color/defaultPrimary`의 여덟 도형은 모두 같은 네 정지점을 쓴다.
위치 0은 창 또는 개별 재질의 강조색, 0.38과 0.72는 원본의 고정 sRGB 파랑
`(0.081204928, 0.509976029, 1)`이며 각 알파는 0.55와 0.13이다. 위치 1은
투명 검정이다. 강한 도형과 옅은 도형의 불투명도는 각각 0.40과 0.11이며,
도형 순서와 크기·중심은 개명된 Figma 라이브러리의 `967:5`를 따른다. 해당
문서의 색상 프로파일은 sRGB이다. 기존 `primaryColor`의 알파가 전체 악센트를
제어하는 LVUI 계약을 유지한다. 부분 투명 강조색은 원본의 개별 정지점 알파
결합과 다를 수 있다.

위치·색상은 [원본 스냅샷](../catalog/lvui-material-radial.json)에서 생성한
기본값을 사용한다. 기존 단색 마스크 `render`는 유지하고 `render_colored`가
채색된 래스터를 제공한다. iiGUI에는 틴트를 적용하지 않고 밑그림을 전달한다.
강조색 RGB 변경 시 여덟 도형의 기하 계산은 재사용하나 RGBA 래스터를 다시
만들므로 애니메이션 중 프레임마다 텍스처 업로드가 발생할 수 있다. 메모리와
성능은 실제 GPU에서 측정하지 않았다. Figma 원본의 네 픽셀 표본을 비교한
모델 검사와 헤드리스 장면 검사는 전체 이미지나 GPU 표시의 동등성 검증이 아니다.

## 레거시 경로

이전 `FRAMEWORK LVRS` 경로는 LVRS 팩터리와 옵션을 생성했다. 해당 경로는
제거되었으며, 당시 지원한 입력과 LVUI 경로의 현재 지원 범위를 구분한다.

macOS arm64에서 독립 iiGUI 설치 소비자와 LVUI 다색 재질·팝오버·툴팁 결합을
창 없이 검사했다. 재질의 그리기·프레임 요청·수명도 `iiGUI/NativeMaterialView.h`가 소유한다.
생성된 앱은 LVUI의 `MaterialBinding`에 iiGUI 모델·호스트·래스터 정책을 주입한다.
실제 플랫폼·GPU 검증은 남아 있다.

재질 위젯이 폐기되면 애니메이션을 중단하고 외부 핸들이 남아 있어도 그라데이션·
윤곽 이미지 캐시를 해제한다. macOS arm64 독립 호스트에서 자원 반환을 검사했다.
