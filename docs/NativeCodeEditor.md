# LVUI 독립 CodeEditor와 AOT 연결

`NativeCodeEditor.h`는 LVUI의 [독립 코드 문서·입력 세션](CodeDocument.md)을 iiGUI의
일반 위젯 및 텍스트 셰이퍼에 연결한다. `CodeEditorInput`에 교체 가능한 `source`와
선택적인 `editor`를 주입한다. 편집기가 없으면 읽기 전용이다. 보이는 줄만 읽고 셰이핑하며
선택, 스칼라 이동·삭제, 줄 이동, 복사·잘라내기, 일반 입력과 조합 입력을 처리한다.
복사 출력은 주입할 수 있고, 원본 리비전이 바뀌면 `refresh()`로 명시적으로 갱신한다.
`frame()`과 `snapshot()`은 화면 상태를 문서 편집 없이 조회한다. 실제 문서 리비전이
바뀌지 않은 동일 문자열 교체는 선택을 접을 수 있지만 편집 이벤트를 발행하지 않는다.

한 화면은 최대 512줄과 4 MiB의 텍스트를 읽고, 수평으로 긴 줄은 UTF-16 열 단위의
제한된 구간을 표시한다. 문서의 줄 수가 1억을 넘으면 화면 조회를 거절한다. 커서가 현재
구간 밖으로 이동할 때 구간을 옮긴다. 마우스 휠의 수평 이동이 구간 경계 밖으로 자동
진행하지는 않는다. 확장 자소·단어 이동, 실행 취소, 구문 강조, 줄 번호, 스크롤바,
접근성 및 OS 입력 어댑터의 실기기 검증은 남아 있다.

`LVUI.OwnedCodeDocument.Native`는 실제 창을 열지 않는 iiGUI 위젯 트리에서 표시 범위,
조합 입력, 키 입력, 복사·잘라내기, 충돌, 재진입과 큰 문서 탐색을 검사한다. 이는 실제
플랫폼 입력기나 그래픽 화면의 검증을 뜻하지 않는다.

기본 LVUI 선언의 `CodeEditor`는 이 독립 위젯을 생성한다. `model`은
`CodeDocumentSource`, `editor`는 `CodeDocumentEditor` 인터페이스이며 동일한 메모리
문서나 서로 다른 앱 구현을 사용할 수 있다. `model` 없이 `text`를 주면 LVUI가 자체
메모리 문서를 만들고, `readOnly: true`면 편집기를 연결하지 않는다. `model`과 `text`,
`height`와 `editorHeight`는 각각 동시에 사용할 수 없다. 주입 문서의 교체 가능한 포트는
생성 시 정해지며 포인터 자체의 반응형 교체는 아직 지원하지 않는다.

```lvui
import vm EditorModel
Window {
    CodeEditor(model: vm.document, editor: vm.document, sourceRevision: vm.revision,
               width: vm.width, height: 160, onDocumentEdited: vm.changed,
               onSelectionChanged: vm.selectionChanged)
}
```

`sourceRevision`이 바뀌면 기존 위젯의 문서 스냅샷을 새로 읽는다. 위젯에서 발생한
같은 리비전의 확인은 재진입 없이 처리한다. 크기, 글꼴, 여백, 색상, 활성 상태와 접근성
이름의 지원 입력은 반응형으로 갱신된다. 이벤트는 전체 텍스트 복사 대신
`CodeDocumentChange`와 `CodeSelection`을 전달한다. `text`와 `readOnly`는 초기 값이며,
지원하지 않는 속성이나 자식 뷰는 컴파일 오류로 표시한다. 생성 코드의 컴파일과 반응형
갱신은 `LVUI.OwnedCodeDocument.Codegen`과 `LVUI.OwnedCodeDocument.Generated`가 검사한다.
편집 이벤트에서 발생한 외관 변경과 새로운 소스 리비전은 현재 이벤트 전달이 끝난 뒤
적용한다. 같은 이벤트 안에서 문서나 선택을 위젯을 통해 다시 편집하는 요청은 거절한다.

아래는 제거된 레거시 AOT 생성 경로의 역사적 기록이다.

`FRAMEWORK LVRS`의 CodeEditor 예약어는 `lvrs::CodeEditor::create`를 직접 호출하는 C++23을 생성한다. 런타임 파서나 스크립트 실행기를 추가하지 않는다. `model`은 scoped backend의 `shared_ptr<TextDocumentModel>`, `text`는 초기 문자열이다. 두 입력은 함께 사용할 수 없다. `height`와 `editorHeight`도 같은 높이를 지정하므로 함께 사용하지 않는다.

문자·숫자·불리언·색상의 공개 지원 입력은 생성한 options에 명시적으로 반영한다. Core Observable 바인딩은 같은 편집기 인스턴스의 options나 `set_text`/`set_cursor_position`을 갱신한다. 전체 모델을 교체하는 반응형 바인딩은 제공하지 않는다. cursorPosition의 백엔드 값은 음수가 없는 정수 위치로 제공한다.

`onDocumentEdited`는 변경 범위와 모델 상태를 받으며 전체 문자열 복사를 피한다. `onTextEdited`와 `onSubmitted`는 요청된 경우 전체 문자열을 구성한다. `onSelectionChanged`는 UTF-16 범위를 받는다. 모델 통지 안에서는 문서 읽기만 허용하므로 동기적으로 문서를 재편집하지 않는다.

이 레거시 경로와 그 옵션은 제거 대상이며 LVUI 독립 API의 구현이나 실행 의존성이 아니다.
