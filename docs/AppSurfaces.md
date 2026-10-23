# AppHeader와 AppCard 코드 생성

FRAMEWORK LVRS에서 두 예약어를 실제 AppHeader/AppCard::create C++ 호출로 생성한다.
자식은 같은 빌더에 구성하며 iiGUI의 기본 콘텐츠 슬롯으로 액션 행 또는 카드 본문에
붙인다. 기존 백엔드를 재사용하고 새 OS 컨트롤·소스 해석기를 만들지 않는다.

title/subtitle와 enabled, width/height, 배경·텍스트 색상, AppHeader의 compact·
menuVisible·각 여백/간격, AppCard의 padding·spacing·cornerRadius·shapeStyle을 받는다.
Core Observable인 직접 C++ 멤버는 BindingScope에 구독하고 plain 멤버는 구성 시 읽는다.
shapeStyle의 상수는 roundRect/shapeRoundRect 또는 cylinder/shapeCylinder이고 C++
멤버는 AppCardShape 타입이다. 색상 상수는 RGB/RGBA HEX이며 C++ 색상은 WidgetColor이다.

onMenuClicked는 지연된 C++ 메서드 참조이다. 잘못된 색상·음수 치수·잘못된 enum·호출식과
미구현 motionEnabled는 소스 진단이다. 지원하지 않는 바인딩을 상수로 동결하지 않는다.

tests/compiler.cpp의 lvrs_app_surfaces와 실제 NativeAppSurfaces 예제의 C++ 빌드·
설치 소비자 빌드로 연결한다. 앱·테스트·self-test 실행은 전체 작업 마지막에 모은다.
지원 경계와 자연 크기·수명은 [LVRS 계약](../../LVRS/docs/native/AppSurfaces.md)을 따른다.
