# LVUI 작업 지침

C++23과 표준·iiSDK의 공개 인터페이스를 사용한다. SDK 소스와 헤더는 src/에서 함께 유지한다. 새 네이티브 경로에 Qt 의존성을 추가하지 않는다.

LVUI는 `.lvui` 선언을 네이티브 C++로 컴파일하는 독립적인 선언형 UI 언어이다.
LVUI는 지원하는 사용자 맞춤 위젯의 코드·상태·동작·그리기를 직접 소유하며,
iiGUI가 제공하는 운영체제별 창·입력·그리기 표면을 사용한다. 레거시 LVRS의
iiGUI 기반 네이티브 구현과 자산은 LVUI 내부로 복제·리팩터링할 수 있지만, 실행
시 LVRS 위젯을 참조하지 않는다. LVUI의 빌드·공개 타입·링크·실행은 LVRS와
Qt/QML에 의존하지 않아야 한다. iiGUI 호스트는 앱에서 조합할 수 있는 경계로 둔다.

기존 `FRAMEWORK LVRS` 생성 경로는 제거했으며 명시적 선택은 오류로 진단한다.
기능별 이식과 패키지 검증은 계속 진행 중이다. 현재 상태와 이전 순서는
[독립 구현 현황](docs/LVUIIndependence.md)과
[네이티브 이전 우선순위](docs/NativeMigrationRoadmap.md)를 따른다.

빌드·설치 스테이징·검사는 저장소의 build/ 안에서 수행한다. 소스 변경 시 관련 docs/와 tests/를 함께 갱신한다. README.md는 영어, docs/ 문서는 한국어로 작성한다. 기존 작업과 사용자 파일을 보존한다. 플랫폼별 소스 이식과 실제 실행 검증을 구분한다.

<!-- workspace-docs:agent:start -->
## 문서 구조와 에이전트 탐색

- 프로젝트 루트의 `README.md`는 사람용 영문 진입 문서이며 `AGENT.md`는 에이전트 진입점이다.
- 하위 인간용 Markdown과 구조화된 agent JSON을 함께 유지한다. Markdown이 원본이며 JSON은 원문 해시와 줄 범위를 가진 파생 문서이다.
- 먼저 [docs/agent-index.json](docs/agent-index.json)에서 관련 문서와 JSON 투영을 찾는다. 원문 SHA-256이 다르면 현재 Markdown을 읽고 투영을 갱신한다.
- JSON의 규칙 후보는 검색용 어휘 일치 결과이다. 과거 기록·예제·실패 상태를 현재 지시나 검증 완료로 자동 해석하지 않는다.
- 기존 프로젝트별 규칙, 동결된 증거, 라이선스, 외부 원본, 사용자 데이터와 다른 작업의 변경을 보존한다.
- 이 문서 재편 지침은 과거 작업의 기능 구현·배포를 자동 재개하라는 요청이 아니다. 현재 사용자의 작업 범위를 우선한다.
- 문서를 수정하면 JSON 쌍·색인을 함께 갱신하고 검증한다. 기존 소스 변경에 대한 빌드·테스트 규칙은 계속 따른다.

Workspace에서 이 프로젝트의 문서를 갱신하는 명령은 다음과 같다.

```sh
python3 ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. sync --project SDK/LVUI
```
<!-- workspace-docs:agent:end -->
