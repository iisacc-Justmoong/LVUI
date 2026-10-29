# C++ 이력 모델을 표시하는 뷰

이력 저장·중복 판단·한도·undo/redo는 LVRS의 네이티브 `ModelUndoStack`이 수행한다.
LVUI는 C++ 메서드를 이벤트에 연결하고 모델의 Core 관측 결과를 화면에 바인딩한다.
QML 배열이나 화면 언어의 이력 조작 메서드를 만들지 않는다.

```lvui
import vm NativeHistoryBackend
Window { VStack {
    Label(text: vm.history.undo_depth())
    Label(text: vm.history.redo_depth())
    PushButton(text: "Undo", enabled: vm.history.can_undo(), onClick: vm.undo)
    PushButton(text: "Redo", enabled: vm.history.can_redo(), onClick: vm.redo)
} }
```

C++ 컨트롤러는 원본 데이터를 언제 기억하고 적용할지 결정한다. 이력 모델의 상태 조회와
화면 재배치는 파일 저장이나 생성 작업의 실행을 암묵적으로 호출하지 않는다.
모델을 직접 가져온 읽기 전용 표시도 지원한다.

```lvui
import history lvrs.ModelUndoStack
Window { Label(text: history.undo_depth()) }
```

모델의 getter 내부 Core 관측은 기존 NativeMethods 경로가 수집하므로 별도 QML
revision 전달이나 문자열 속성 조회가 필요하지 않다. 한도 변경으로 실제 이력이 잘리면
깊이와 버튼 가능 여부도 갱신하도록 모델이 stack 변경을 발행한다.

이동된 데이터와 수신자 오류를 함께 처리하는 `SnapshotTransfer` 및 값·수명 계약은
[LVRS ModelUndoStack](../../LVRS/docs/native/ModelUndoStack.md)을 따른다.
두 fixture의 실제 생성 코드를 일반·메모리 검사 구성의 컴파일 대상으로 유지한다.
LVRS의 모델·버튼·생성 뷰 실행 검사는 최종 일괄 단계에 남긴다.

2026-10-27의 두 구성 컴파일·링크, 팩터리 64개의 동일 생성 코드, 재배치 소비자와
LVRS 검사 55개의 등록 상태는
`SDK/LVRS/build/lvui-completion/snapshot-history/verification-20261027.json`에 기록한다.
실행하지 않은 LVRS 검사를 통과로 보고하지 않는다.
