# 네이티브 머티리얼 AOT

FRAMEWORK LVRS에서 MaterialSurface, PanelMaterial, WindowMaterial을 C++23로 생성한다.
자식 View는 기존 스코프와 네이티브 루트에 남는다. 생성/갱신은 VM 없이 직접 C++ API이다.

```lvui
import vm MaterialBackend
Window {
 PanelMaterial(width: 380, height: 204, density: .Glass25,
               primaryColor: vm.accent, backdrop: .PreviousLayers, motion: vm.motion) {
  Label(text: "Native material")
 }
}
```

지원 입력은 density, primaryColor, color, borderColor, backdropBaseColor, tintOpacity,
blurRadius, radius, innerHighlightOpacity, innerHighlightOffsetY, borderWidth, intenseOpacity,
faintOpacity, shadowRadius, shadowOffsetY, shadowOpacity, blurEnabled, motionEnabled, enabled,
backdrop, outlinePath, width, height, motion 및 공통 visible/flex이다.
optional 속성을 생략하면 종류·밀도 기본값을 따른다. density는 .Dense75/.Glass25,
backdrop은 .None/.Content/.PreviousLayers이며 Qt Item 포인터를 받지 않는다. motion은
공유 MotionPreferences이다. 색상은 HEX 문자열 또는 typed C++/Core Observable이고
나머지 지원 값도 기존 객체의 옵션 변경으로 반응형 연결된다.

원본의 임의 Item backdropSource/backdropBackground를 조용히 무시하지 않으며 해당 입력은
진단으로 거절한다. 임의 소스 캡처, 전체 상속·글로벌 Theme와 플랫폼 동등성은 후속 작업이다.
모든 검사와 앱 실행은 전체 작업 마지막 일괄 단계까지 보류한다.
