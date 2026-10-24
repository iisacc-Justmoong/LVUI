# CodeEditor AOT 연결

`FRAMEWORK LVRS`의 CodeEditor 예약어는 `lvrs::CodeEditor::create`를 직접 호출하는 C++23을 생성한다. 런타임 파서나 스크립트 실행기를 추가하지 않는다. `model`은 scoped backend의 `shared_ptr<TextDocumentModel>`, `text`는 초기 문자열이다. 두 입력은 함께 사용할 수 없다. `height`와 `editorHeight`도 같은 높이를 지정하므로 함께 사용하지 않는다.

문자·숫자·불리언·색상의 공개 지원 입력은 생성한 options에 명시적으로 반영한다. Core Observable 바인딩은 같은 편집기 인스턴스의 options나 `set_text`/`set_cursor_position`을 갱신한다. 전체 모델을 교체하는 반응형 바인딩은 제공하지 않는다. cursorPosition의 백엔드 값은 음수가 없는 정수 위치로 제공한다.

`onDocumentEdited`는 변경 범위와 모델 상태를 받으며 전체 문자열 복사를 피한다. `onTextEdited`와 `onSubmitted`는 요청된 경우 전체 문자열을 구성한다. `onSelectionChanged`는 UTF-16 범위를 받는다. 모델 통지 안에서는 문서 읽기만 허용하므로 동기적으로 문서를 재편집하지 않는다.

현재 지원 API와 원본 대비 미구현 경계는 LVRS의 `docs/native/CodeEditor.md`에 기록되어 있다. RichText TextEditor를 이 PlainText 위젯으로 대체하지 않으며 지원하지 않는 속성과 자식 뷰는 컴파일 시 진단한다. 네이티브 CodeEditor 갤러리는 실제 생성된 C++를 컴파일·링크한다. 테스트/앱 실행은 사용자 요청에 따라 전체 이식의 마지막에 일괄 수행한다.
