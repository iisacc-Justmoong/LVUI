# 앱 제공 편집 항목과 비파괴 미리보기

`MenuActionStateBinding`은 앱이 제공한 메뉴 명령의 활성·선택·표시 상태를 표시용
값으로 변환한다. 상태 원본과 변경 알림, 명령 실행은 앱이 소유한다.
`MenuSubmenuItemsBinding`은 앱이 제공한 안정적인 항목 ID와 순서를 보존하고
중복 ID를 거부한다. 두 바인딩 모두 임의의 전역 메뉴나 앱 객체를 찾지 않는다.

`MenuCommandParameterForm`, `NonDestructivePreviewPanel`,
`MaskRefinementControls`, `LayerContentOptionsPanel`, `FilterGalleryPanel`,
`ColorProfilePicker`, `SoftProofSettingsPanel`, `TimelineEditOptionsPanel`,
`ExportFormatOptionsPanel`은 앱이 공급한 항목과 제약을 편집하는 뷰와 연결한다.
필드의 이름, 선택지, 범위와 지원 여부는 앱이 제공하며 LVUI가 특정 제품의
필터·프로파일·파일 형식을 추측하거나 고정하지 않는다. 뷰의 시각적 요소는 LVUI의
기존 공통 컴포넌트와 디자인 토큰을 사용하고, 앱은 자신의 C++ 뷰모델을 연결한다.

편집은 원본과 분리된 임시 값에서 시작한다. 변경 시 미리보기가 활성화되어 있으면
현재 수명의 미리보기 요청을 반환한다. 앱이 임시 결과를 계산하고 표시한다. 이전
수명이나 이전 편집 세대의 결과는 새 값을 덮어쓰지 못한다. 적용은 앱에 현재 값을
확정하도록 요청하며 앱의 성공 응답이 도착하기 전에는 완료가 아니다. 취소는 임시
값을 원본으로 되돌리고 앱에 미리보기 폐기를 요청한다. 앱은 문서 변경과 작업 취소,
비동기 실행, 실패 처리 및 자원 수명을 소유한다.

항목 ID는 한 패널에서 고유해야 한다. 입력 타입, 필수값, 앱이 지정한 숫자 범위와
선택지는 편집 결과를 게시하기 전에 검사한다. 잘못된 입력은 이전 상태를 보존하고
명시적인 오류를 반환한다. 외부 호출과 스레드 동기화는 호스트가 제어한다.

## C++ 상태 및 메뉴 계약

`#include <LVUI>` 또는 `<LVUI/EditorPanels.h>`를 사용한다. 메뉴 상태는 앱의
`MenuActionState`에서 `MenuEntry`로 변환한다. `checked`는 기존 메뉴의 선택 표시
상태에 대응하며 별도의 체크 표시 아이콘은 앱이 지정한다. 하위 메뉴 바인딩은
일반 항목, 중첩 메뉴와 구분선의 종류를 보존한다.

편집 패널은 `EditorPanelRequest`를 입력받아 새 `EditorPanelState`와 `EditorEffect`
목록을 반환한다. `begin`은 원본 스냅샷을 고정하며, `edit`은 초안만 변경한다.
`NonDestructivePreviewPanel`은 유효한 초안 변경마다 미리보기를 요청한다. 다른
패널도 앱이 `preview_enabled`를 지정하면 같은 미리보기 계약을 사용할 수 있다.
필수 텍스트가 비어 있는 초안은 편집할 수 있지만 적용할 수 없으며, 앱이 제공한
`validation_error`가 남은 필드도 적용할 수 없다. 지원하는 입력은 텍스트, 정수,
실수, 불리언과 앱 제공 선택지이다.
정수 필드는 부동소수점 경계 외에 정확한 64비트 정수 최솟값과 최댓값도 받는다.
같은 방향에 두 종류의 경계를 동시에 지정하면 모호한 입력으로 거부한다.

미리보기 효과에는 앱 활성화 ID와 원본·편집 리비전이 포함된다. 앱은 해당 토큰을
사용해 `preview_ready` 또는 `preview_failed`를 제출한다. 이전 결과는 거절하며
앱은 거절된 비동기 결과의 자원도 해제해야 한다. 교체하거나 실패한 미리보기는 이전
결과를 폐기한다. `apply`는 `commit`을 요청하며 갱신 중인 이전 미리보기도 폐기한다.
앱이 `apply_succeeded`를 제출해야 완료된다. 실패는 초안을 유지하고 미리보기가
없으면 다시 요청한다. 확정 중에는 취소하지 않으며, 일반 취소와 확정 성공은 임시
미리보기 폐기 효과를 반환한다. 앱은 효과를 해석하고 대상 문서와
이력에 반영한다.

필드의 항목 ID와 스키마가 바뀌면 앱은 새 활성화·원본 리비전으로 시작한다.
바인딩은 값의 사본만 다루고 앱 객체를 참조하거나 호출하지 않는다. 앱은 원본
변경 이벤트를 구독하고 필요한 바인딩만 다시 실행하며, 폴링은 하지 않는다.

## 선언형 View와 Python

`compile_editor_panel_views` 또는 `lvui-compile --editor-panels`는 아홉 개 패널
선언과 `EditorTextControl`, `EditorToggleControl`, `EditorRangeControl`,
`EditorChoiceControl`, `EditorAutoFields`를 해당 소스의 어휘 스코프에 추가한다. CMake에서는
`lvui_generate(... EDITOR_PANELS ...)` 또는 `lvui_add_application(... EDITOR_PANELS ...)`을
선택한다. 기존 선언과의 이름 충돌을 피하도록 기본 컴파일에는 삽입하지 않는다.
각 패널은 `fields`와 `preview` 슬롯 및 앱이 제공한 제목·버튼 문구·적용 가능 여부와
적용·취소 콜백을 받는다. `fields: [EditorAutoFields(model: vm.autoModel)]`를 전달하면
필드 종류마다 View를 작성할 필요가 없다. 기존의 직접 구성한 슬롯도 사용할 수 있다.
아홉 패널은 공통 표면을 사용하며 갤러리 썸네일과 미리보기처럼 스키마 외의 콘텐츠는
앱이 슬롯으로 제공한다.

`EditorAutoFormBinding::execute({fields, on_edit})`는 검증된 앱 스키마를 화면용 행으로
투영한다. 결과를 앱이 소유한 `EditorAutoFormModel`의 `fields` Observable에 게시하면
LVUI가 안정적인 필드 ID를 기준으로 행을 만든다. 양쪽 경계가 지정되고 서로 다르며 기존
슬라이더 범위인 ±10¹² 이내인 실수는 슬라이더, 나머지 실수와 텍스트·정수는 입력 필드,
불리언은 체크박스, 선택지는 목록
선택기로 구성한다. 레이블은 접근성 이름에도 사용한다. 표시·활성 여부와 앱의 검증
메시지를 반영하며 선택 이벤트는 표시명이 아닌 선택지 ID를 전달한다.
`vm.autoModel`은 예시처럼 앱이 수명을 관리하는 `std::shared_ptr<EditorAutoFormModel>`
등의 명시적인 객체 핸들로 전달한다.

호스트는 받은 `EditorFieldEdit`의 `field_id`, `kind`, 선택적 `value`, `raw_text`를
처리한다. 정수는 64비트 정확도를 보존하고, 미완성 또는 잘못된 숫자는 `value` 없이
원문을 전달한다. 앱은 유효한 값을 해당 패널의 `edit` 이벤트에 제출하고 거절된 값과
범위·필수값 오류를 표시할 메시지를 제공한다. 앱이 새 스키마·초안을 투영해 Observable에
게시하면 항목 변경이 갱신된다. 새 행의 콜백은 전달받은 이벤트 수신자를 보유하므로 앱은
그 수신자의 수명을 뷰보다 길게 유지하거나 약한 수명 보호를 사용해야 한다. 이벤트와
공유 상태의 실행 순서는 앱이 통제한다.

선택값이 비어 있으면 빈 행이 미선택을 나타낸다. 필수가 아닌 선택지는 이 빈 행으로
값을 지울 수 있다. 기존 네이티브 선택기의 한도는 빈 행을 포함해 400개이고 선택지
레이블은 각각 8,192바이트, 전체 1 MiB 이하이다. 반복 행 ID는 1,024바이트 이하이며
필드 레이블과 검증 메시지는 각각 65,536바이트 이하이다. 이 한도를 초과한 스키마는
불완전한 뷰를 만드는 대신 투영 단계에서 오류를 반환한다.

```lvui
MenuCommandParameterForm(title: vm.title,
    fields: [EditorAutoFields(model: vm.autoModel)], preview: [],
    applyLabel: vm.applyLabel, cancelLabel: vm.cancelLabel,
    applyEnabled: vm.canApply, onApply: vm.apply, onCancel: vm.cancel)
```

Python에서 `load_editor_library(절대_경로)`로 네이티브 브리지를 명시적으로 불러온
뒤 `compile_native(source, editor_panels=True, library=library)`로 동일한 View를 컴파일한다.
같은
이름의 Python 바인딩 객체의 `execute`에 상태·이벤트 딕셔너리를 전달할 수 있다.
반환된 `state`와 `effects`는 C++의 동일한 상태 전이에서 생성된다. C ABI 문자열과
입력 배열은 호출자가 소유하며 반환 객체는 명시적으로 해제한다. Python의
`EditorAutoFormBinding(library).execute(fields)`는 동일한 자동 필드 투영의 데이터
스냅샷을 반환한다. Python API는 View의 이벤트 콜백이나 네이티브 위젯 호스트를
암묵적으로 생성하지 않는다.

시안은 [LVUI Figma의 편집 패널](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI?node-id=2059-227)이며
각 컴포넌트에 기능과 책임을 한국어 Annotation으로 기록했다. 시안에 표시된 항목과
값은 앱이 제공하는 데이터의 예시이며 필수 옵션 목록이 아니다.

작업 공간의 문서 동기화 도구가 없는 동안 이 문서와 색인의 JSON 투영은 원문 블록,
줄 범위와 해시를 확인하여 수동 생성하고 `manual-projection`으로 표시한다.

호스트 연결 예시는 [LVUI 선언과 뷰모델](../tests/fixtures/EditorPanelViews.lvui),
[자동 필드 선언](../tests/fixtures/EditorAutoPanel.lvui),
[C++ 상태 전이](../tests/editor_panels.cpp), [Python 상태 전이](../tests/test_editor_panel_python.py)에서
확인할 수 있다. 앱은 예시와 달리 자신의 실제 항목, 검증과 작업 효과를 제공한다.

헤드리스 위젯 검사는 iiGUI의 비공개 테스트 호스트를 사용한다. CMake의
`LVUI_IIGUI_HEADLESS_TEST_LIBRARIES`에 `LVUI_IIGUI_SOURCE_DIR`과 같은 리비전으로
빌드한 iiGUI 런타임 라이브러리 목록을 명시하면 해당 통합 검사가 활성화된다.
목록을 제공하지 않아도 생성된 C++의 컴파일과 나머지 패널 계약은 검사한다.

공통 텍스트·토글·범위 및 자동 선택 컨트롤은 앱이 제공한 라벨을 접근성 이름으로도
사용한다. 앱이 직접 전달하는 선택기와 미리보기 View의 접근성 정보는 앱이 제공한다.

기존 상태 전용 및 컴파일 전용 생성 View 검사에는 iiGUI 모듈 헤더 경로를 명시한다.
이 검사는 GUI 호스트를 실행하지 않고 생성 코드와 상태·바인딩 계약을 확인한다.
