# AOT·Cocoa Hello World 검증


> 이 문서는 초기 AppKit 컨트롤 구현의 검증 기록이다. 현재 구현은
> [LVRS 자체 위젯](LVRSNative.md)과 iiGUI GPU 렌더러를 사용하며 OS 컨트롤을 생성하지 않는다.

검증일은 2026-10-22이며 macOS arm64, AppleClang 21, C++23 환경이다.
LVUI는 Debug, iiGUI와 Hello World는 Release 구성이다.

## 빌드와 회귀 검사

`LVUI/build`에서 코드 생성기·라이브러리·테스트를 빌드하고,
`iiGUI/build`에서 AppKit 백엔드와 생성한 Hello World를 빌드했다.
Qt5·Qt6 패키지 탐색을 비활성화한 구성이다.

- LVUI 코드 생성 검사: 직접 위젯 호출, 결정적인 출력, 입력 타입·지원 범위,
  소스 행·열, 백엔드 메서드 참조, UTF-8·문자열 escape·숫자 정규화를 검사한다.
- 명령행·빌드 도구 검사: 공백 경로, 실패 시 빈 출력, 잘못된 인자·include,
  1 MiB 한계, 실패한 생성이 기존 산출물을 보존하는지 검사한다.
- LVUI CTest 20개 통과: 기존 파서·전체 예약어·소스 조사와 AOT 검사 5개 그룹을 포함한다.
- iiGUI CTest 35개 통과: 새 위젯 검사 6개와 Hello World 실행을 포함하며,
  기존 호스트·Core I/O·작업·프레임·Metal·Vulkan·캔버스 경로도 통과했다.
- `tests/widgets.mm`는 실제 AppKit 객체에 `isKindOfClass`를 적용하고
  Auto Layout 모호성·너비 변경·작업자 접근 거부·실패 복구·정체성 교체를 검사한다.
  반복 클릭·비활성 버튼·스코프 소멸·자기 해제·예외 후 종료도 검사한다.
- 별도의 Debug ASan·UBSan 빌드에서 새 위젯 검사 6개와 Hello World 실행, 총 7개가 통과했다.
  위젯과 Metal 캔버스 사이의 성공·실패 교체도 검사한다.

## 실제 Hello World

`iiGUI/examples/HelloWorld/HelloWorld.lvui`를 빌드 호스트의 `lvui-compile`로 생성하고,
출력 `HelloWorld.generated.h`를 C++23으로 컴파일하여 `HelloWorld.app`을 만들었다.
소스에 실제 위젯 트리와 두 개의 C++ 메서드 참조가 있다.
네이티브 실행 파일은 Mach-O arm64이며 마운트된 Cocoa 위젯은 9개이다.

실행 시 실제 클래스는 `NSStackView` 2개, `NSTextField` 5개, `NSButton` 2개이다.
초기 콘텐츠 720×420에서 820×460으로 변경하고 루트 너비도 820으로 반영됨을 검사했다.
250 ms Core 타이머, 작업자 계산 결과 42의 UI 반영, 두 번의 네이티브 버튼 동작,
네이티브 닫기 동작을 거쳐 `main`으로 정상 반환했다.
가져온 백엔드 생성·소멸 횟수는 각각 1이며 오래된 핸들 접근은 거부된다.

```text
HELLO_WORLD_AOT_OK native=Cocoa timer=true background=42 resize=true actions=2 closed=true scopes=1/1
```

## 설치 소비자와 실행 의존성

재현 절차는 iiGUI의 `tests/aot_package.cmake`와 `docs/NativeWidgets.md`에 있다.
설치된 Core와 이번 LVUI·iiGUI 패키지를 스테이징하여 위치를 이동한 후,
설치된 예제를 복사한 별도 프로젝트를 exported target만으로 빌드한다.
소비자의 소스와 `lvui-compile`을 잠시 사용할 수 없는 경로로 이동하여 실행하고 복원한다.
검증 결과·링크 의존성·심볼·JUnit 로그는 소비자 검증 디렉터리에 남긴다.
`iiGUI/build/aot-consumer`에서 실제로 수행했고 설치 소비자 CTest 1개가 통과했다.
소스·도구가 없는 실행과 파서 심볼·Qt 링크 부재도 검사했다.

증거 파일은 다음과 같다. 모두 각 저장소의 `build/` 아래에 있다.

- `LVUI/build/aot-tests.xml`: 최종 LVUI 20개 검사.
- `LVUI/build/aot-documentation-qa.json`: Markdown 19개·상대 링크 130개·링크된 앵커 17개의 검사.
- `iiGUI/build/aot-regression-tests.xml`: 최종 iiGUI 35개 검사.
- `iiGUI/build/aot-sanitizer/build/aot-sanitizer-tests.xml`: ASan·UBSan 7개 검사.
- `iiGUI/build/aot-consumer/verification.json`: 패키지 재배치·독립 실행·의존성 검사 결과.
- `iiGUI/build/aot-consumer/build.log`, `runtime.log`, `tests.xml`, `dependencies.txt`, `symbols.txt`.

일반 `~/.local` SDK 설치본은 교체하지 않는다. 이는 설치 패키지 소비 검증이며
Core·MoltenVK까지 내장한 독립 배포 번들의 패키징 검증은 아니다.
Hello World는 GPU 캔버스를 할당하지 않으며 기존 패키지가 링크하는 MoltenVK는 외부 네이티브 키트이다.
실행 파일에서 Qt 의존성과 LVUI 파서·컴파일러 심볼이 없어야 한다.

## 구현 범위

이 문서의 초기 단계 기록은 당시 macOS AOT 생성·마운트·실행을 다룬다.
현재 LVRS 경로는 iiGUI 자체 C++ 위젯을 단일 Cocoa/GPU 표면에 그리며
NSButton·NSTextField 같은 OS 컨트롤을 래핑하지 않는다. 후속 Alert의 실제 그림자와
프레임 모션·공유 모션 설정·설치 소비자 결과는
[현재 LVRS 검증 기록](../../LVRS/docs/native/Verification.md)과
[AOT 지원 계약](LVRSNative.md)을 기준으로 한다.
모든 예약 컴포넌트의 위젯 구현, Windows·Linux 네이티브 위젯 마운트,
일반 반응형 바인딩과 LVUI GPU·셰이더 코드 자동 생성을 검증한 것은 아니다.
