# 예약어·컴포넌트 프런트엔드 검증

이 문서는 앞선 프런트엔드 작업의 검증 기록이다. 이후 구현한 AOT·Cocoa 실행의
검증은 [NativeAOTVerification.md](NativeAOTVerification.md)에 별도로 기록한다.

검증일은 2026-10-22이며 실행 환경은 macOS arm64, AppleClang 21, C++23 Debug이다.

## 소스 조사 범위

저장소 전체의 코드·빌드 파일 347개도 읽고 해시를 기록했다.
모듈 코드 209개, 예제 44개, 테스트 78개, 빌드·도구 16개이며
`build/lvrs-repository-source-audit.json`에 경로와 분류를 남겼다.
예제 애플리케이션과 테스트 전용 화면 이름은 SDK 모듈의 컴포넌트 이름에 포함하지 않는다.

LVRS 커밋 `13b657b21a8ca9babe40ac790bb547c3f7758eea`의 `src/` 소스 213개 파일을
읽고 CMake 등록·QML 루트 상속·인라인 선언·C++ 등록 타입을 대조했다.
조사 선언은 149개이며 시각 선언 108개, 시각 보조 3개, 비시각 선언 38개이다.
시각 선언의 중복 이름 하나를 합쳐 `View` 예약어 107개를 등록했다.
기본 언어 예약어 6개까지 포함한 전체 예약어는 116개이다.

조사 결과와 LF 개행으로 정규화한 SHA-256은 `catalog/lvrs-components.json`,
실제 C++ 테이블은 `src/Language/Keywords.def`, 사람이 읽는 목록은 [ReservedWords.md](ReservedWords.md)에 있다.
LVRS 소스와 작업 트리는 수정하지 않았다.

## 빌드·테스트

- `cmake --build build --parallel 4`: 네이티브 라이브러리·테스트·`lvui-check` 빌드 성공.
- CTest 15개 그룹 통과: 기존 파서 4개, 컴포넌트 파서 6개, 스냅샷·실제 소스 조사·조사 도구 3개, 예제 2개.
- 107개 시각 예약어 모두를 실제 생성 구문으로 파싱하고, 전체 116개 예약어의 변수·import 이름 충돌을 검사했다.
- UTF-8·CRLF·문자열 escape·주석·자식 뷰·입력·호출과 참조·중복 입력·잘린 소스·입력 한계를 검사했다.
- 조사 도구 단위 검사 6개는 인라인·C++·비시각 분류, 문자열 오인 방지, 알 수 없는 기반 타입,
  누락된 CMake 등록, 소스 내용 변경, 중복 이름의 원본 유지와 CRLF 정규화를 검사한다.
- 실제 `lvui-check`로 12개 뷰 예제를 검사하고 예약어 충돌의 `1:5` 위치·오류 종료 1·사용법 종료 2를 확인했다.

## 설치 소비자

`build/install`에 스테이징하고, 그 설치 패키지만 참조하는 별도 C++23 소비자를
`build/reserved-word-consumer/build`에서 빌드했다. 소비자 테스트 2개가 통과했다.
설치된 헤더와 라이브러리로 107개 시각 예약어, 전체 116개 예약어, 새 `View` 트리와
기존 `Document` API를 검사하고, 설치된 `lvui-check`와 설치된 예제 파일도 실행했다.

`otool -L`에서 검사 도구와 소비자는 libc++·libSystem만 사용하며 Qt 라이브러리는 없다.
일반 `~/.local` 설치본은 교체하지 않았다.

## 증거와 구현 경계

- `build/reserved-words-ctest.txt`
- `build/reserved-words-consumer-ctest.txt`
- `build/reserved-words-verification.json`
- `build/lvrs-repository-source-audit.json`

이번 증거는 실제 어휘·구문 분석, 예약어 판정, 소유 `View` AST, 설치 패키지의 검증이다.
OS 위젯·가속 코드 생성과 iiGUI 마운트·런타임 동작 구현을 입증하지 않는다.
Windows·Linux의 빌드 또는 실행은 이번 Mac 검증에 포함하지 않았다.


## Alert·AlertButton AOT 추가 검증 — 2026-10-23

`Alert`과 `AlertButton`을 단순 예약어 등록에서 실제 LVRS C++ 생성 경로로 연결했다.
현재 전체 LVUI 검사 30개가 통과했으며 기록은 `build/alerts-full-tests.xml`이다.
원본 속성과 별칭 충돌, 정수 액션 개수, 금지 자식, 이미지 입력, shape와 잘못된 메서드 호출을
진단한다. 생성 코드는 C++23의 종속 `requires`로 ButtonEvent 인자 메서드와 인자 없는
메서드를 선택하고 부모 스코프 인스턴스를 재사용한다. 런타임 이름 조회를 추가하지 않는다.

원본 LVRS와 네이티브 API를 합친 소스 229개를 다시 조사했다. 선언 149개·시각 예약어
107개·시각 수정자 3개·전체 예약어 116개라는 분류는 유지한다. 새 네이티브 API 네 파일의
해시를 포함하며 기존 원본 QML 선언을 제거하거나 구현 완료로 바꾸지 않았다.
`examples/NativeAlertGallery`의 생성 헤더는 실제 C++23 앱으로 빌드되어 여섯 액션 위치와
독립 버튼, 메서드→클릭 순서, 원본 SVG, 실제 GPU 흐림과 Core 수명을 검사한다.
전체 이식 상태와 플랫폼 범위는 [LVRS 검증 기록](../../LVRS/docs/native/Verification.md)을 따른다.
