# 네이티브 C++ 위젯·Core 앱 연결 검증


> 구현 변경: 이 문서의 초기 Cocoa 컨트롤 경로는 [현재 LVRS 자체 위젯 구현](LVRSNative.md)으로 대체되었다.
> Core의 앱·스코프 소유 구조는 유지하며, 현재 레이아웃·입력·그리기는 iiGUI가 직접 처리한다.
> 아래 AppKit 컨트롤 설명과 검증 수치는 이전 구현 시점의 기록이다.

검증 환경은 2026-10-22, macOS arm64, AppleClang 21, C++23이다.
이 기록은 앞선 [AOT·Cocoa 구현 검증](NativeAOTVerification.md) 이후의
[Core 앱 소유 구조](NativeWidgetRuntime.md)를 대상으로 한다.

## 확인한 동작

- AOT 결과가 기본 생성 가능한 `final` C++ 위젯이며 `attach`·`widgets`·`detach`를 제공한다.
- Core 설치 패키지의 `ApplicationEntry.cpp`가 최종 실행 파일의 진입점으로 컴파일된다.
- 앱에는 별도 `main`이나 작업자 풀을 만들지 않고 구성 함수를 제공한다.
- iiGUI 호스트가 `Application::event_loop()`와 동일한 객체를 구동한다.
- 타이머와 작업자 결과가 그 Core 루프를 통해 네이티브 컨트롤을 갱신한다.
- 일반 C++로 작성한 위젯도 같은 부착 계약을 사용한다.
- 창이 닫히면 네이티브 핸들을 무효화하고 C++ 백엔드는 Core의 작업 정리·해제까지 유지한다.
- 부착 실패·네이티브 콜백 예외·중복 드라이버·역순 해제에서도 Core가 정상적으로 정리한다.
- 실제 AppKit 객체와 픽셀에서 LVRS 버튼 상태 색상·높이·텍스트 투명도를 확인한다.
- Core의 최종 앱 리소스 경로로 부착한 Pretendard 네 얼굴을 실제 앱에서 사용한다.

Core의 전체 의존성 경계 검사는 GUI·LVUI·LVRS·Qt를 참조하지 않음을 검사한다.
상위 모듈이 Core의 콜백 인터페이스를 구현하며 순환 의존성을 추가하지 않는다.
LVRS 원본은 읽기 참조했고, 외형·글꼴 파일 해시를 iiGUI 스냅샷 검사에 등록했다.

## 설치 소비자

`iiGUI/tests/aot_package.cmake`로 설치 패키지를 새 위치에 재배치한다.
설치된 Core 진입점이 소비자의 컴파일 명령에 포함되는지, 네 글꼴의 번들 해시가 설치 자산과
일치하는지, 최종 실행 파일에 Qt·LVUI 파서 심볼이 없는지 검사한다.
소비자 소스와 AOT 컴파일러를 사용할 수 없는 상태에서 앱의 자체 검증을 실행한다.
설치된 Core 진입점 소스도 실행 중에는 사용할 수 없게 두며, 빌드에 쓰인 C++ 파일이
완성된 앱의 실행 시 입력으로 필요하지 않음을 확인한다.

이 검증은 `build/` 아래에 설치된 패키지를 사용한다. 앱은 Core 동적 라이브러리와
기존 네이티브 Vulkan 키트를 참조한다. Windows·Linux 실행과 독립 배포·서명은 이 Mac 검증 범위에 포함하지 않는다.

최종 검사 수·JUnit·설치 소비자 경로는 `LVUI/build/core-widget-verification.json`에 모은다.

## 최종 결과

| 검사 | 결과 | JUnit 파일 |
| --- | --- | --- |
| iiSDKCore 전체 | 54/54 통과 | `iiSDKCore/build/core-application-tests.xml` |
| LVUI 전체 | 21/21 통과 | `LVUI/build/core-widget-tests.xml` |
| iiGUI 전체 | 40/40 통과 | `iiGUI/build/core-widget-tests.xml` |
| iiGUI·위젯·Hello World ASan/UBSan | 11/11 통과 | `iiGUI/build/aot-sanitizer/build/core-widget-sanitizer-tests.xml` |
| 재배치 설치 소비자 | 1/1 통과 | `iiGUI/build/core-widget-consumer-final/tests.xml` |

모든 검사에서 실패·건너뜀이 0개이다. ASan/UBSan은 iiGUI와 소비자 코드에 적용했으며,
별도로 설치된 Core 동적 라이브러리 전체에 계측을 적용했다는 의미는 아니다.
설치 소비자는 Qt를 찾지 않도록 구성했으며, 최종 Mach-O arm64 바이너리에서 Qt 링크와
LVUI 파서 심볼이 없음을 검사했다. Core의 진입점과 Pretendard 네 얼굴이 최종 앱에 부착된다.

같은 설치 소비자의 `HelloWorld.app`을 자체 검증 인자 없이 직접 실행했다.
접근성 트리와 화면에서 `Hello World · LVRS · iiSDKCore` 창, 타이머,
백그라운드 결과 `42`를 확인했으며, `Say Hello` 버튼을 클릭하자
문구가 `Hello World · Cocoa!`로 바뀌었다. 최종 창은 실행 상태로 유지했다.

현재 AOT 네이티브 구현 범위는 `ApplicationWindow`, `VStack`, `HStack`, `Label`,
`PushButton`이다. 나머지 예약어는 AOT 진단을 반환하며, 이 기록은 전체 107개 예약어의
네이티브 구현이나 범용 반응형 속성 추적을 검증한 것으로 해석하지 않는다.
