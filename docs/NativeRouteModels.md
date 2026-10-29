# 네이티브 페이지와 뷰모델의 수명

LVUI의 `PageRouter(router: vm.router)`는 스코프가 보유한 C++ 라우터를 표시한다.
경로 결정, 논리 이력, 모델 연결, 쓰기 권한과 정리는 LVRS의 네이티브 C++가 수행한다.
LVUI 소스에 별도의 페이지 전환 스크립트나 파일 상태 갱신 메서드를 넣지 않는다.

백엔드는 `PageRouterOptions::view_models`에 기존 `ViewModelRegistry`를 공유한다.
경로의 `metadata` 또는 탐색 매개변수에 `modelKey`/`viewModelKey`, `viewId`,
`writable`/`modelWritable`을 지정한다. 모델 키와 ID의 우선순위, fallback 예외와
원본 QML 비교는 [LVRS 페이지 라우터](../../LVRS/docs/native/PageRouter.md)에 정리한다.

페이지 생성에 성공하여 화면과 이력을 공개한 뒤 모델을 연결한다. 팩토리 실패는
기존 모델 연결을 바꾸지 않는다. 제거한 이력의 마지막 ID 참조에서 연결을 해제하고,
상위 생성 실패나 스코프 종료 때에도 전체 권한을 정리한다. 모델 알림 중 종료는
전용 수명 정리 큐로 마무리한다. 임의 재진입 쓰기를 허용하지 않는다.

`NativeRouteRegistry.lvui`와 `NativeRouteRegistryFixture.h`는 C++ 명령만 가진
백엔드에 PageRouter, 반응형 Label, 쓰기 권한을 따르는 PushButton을 연결한다.
스코프가 각각의 레지스트리를 소유하므로 독립된 뷰 활성화는 권한을 공유하지 않는다.
모델 읽기의 Core 관측 의존성은 생성 바인딩에 수집하며 새 페이지 연결 뒤에 표시값을
갱신한다. 화면 이동 자체는 `file_revision`, StateModel values나 편집 이력을 수정하지 않는다.

이 팩터리는 일반·ASan/UBSan 구성의 C++ 컴파일·링크와 재배치 소비자에 포함한다.
실제 생성 위젯의 작업 흐름, 실패, 수명, 도메인 격리와 문서 비변경 검사는 LVRS의
`tests/native/route_registry_aot.cpp`에 준비한다. LVRS 앱·테스트 실행은 전체 목표의
최종 일괄 단계에 보류하므로 빌드 성공을 동작·성능·전체 플랫폼 검증으로 해석하지 않는다.

2026-10-27에 일반·ASan/UBSan 구성에서 이 팩터리를 포함한 67개 LVRS 생성 팩터리를
컴파일했으며 두 구성의 생성 헤더가 동일함을 확인했다. LVUI 관련 검사 68개씩은 통과했다.
새 모델 연결 검사와 기존 탐색·상태 검사를 합친 LVRS 166개씩은 실행 파일과 명령 등록까지
대조했으며 실행하지 않았다. 재배치 소비자는 팩터리 10개를 포함하여 두 구성에서 링크했고
공개 헤더 10개를 원본과 비교했다. 소스 저장소의 검증 기록은
`SDK/LVRS/build/lvui-completion/page-model-bindings/verification-20261027.json`이다.
