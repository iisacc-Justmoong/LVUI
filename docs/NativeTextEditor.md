# 네이티브 여러 줄 텍스트 편집기

Society의 프로젝트 목표·일정 메모와 Dreamscapes의 생성 프롬프트는 외부 파일을
연결하지 않는 일반 텍스트를 편집한다. `TextEditor`는 이 사용을 위한 독립적인
선언형 View와 설정 계약을 제공한다. iiGUI의 `BasicNativeTextEditor`가 입력 필드 조합과 최소 높이 계산을 소유하며, 생성된 앱은 `TextEditorBinding`으로 iiGUI 구현을 연결한다.
현재 Figma의 TextField 재질과 LVUI 토큰을 공유한다. 입력 영역은 여러 줄, 자동
줄바꿈, 선택, 입력기 조합, 실행 취소와 외부 모델의 이벤트 기반 갱신을 지원한다.
호환되는 iiGUI 호스트에서는 포커스와 커서 이동 시 같은 위젯 트리의 일반 스크롤
조상이 커서 영역을 보이게 한다. 이전 iiGUI 헤더에서는 이 기능이 비활성화된다.
다른 트리나 팝업에 걸친 스크롤 이동은 아직 제공하지 않는다.
여러 줄 편집기에서는 휠·트랙패드 스크롤을 내부 표시 범위에 적용한다. 읽기 전용
텍스트도 스크롤할 수 있으며 원문과 선택은 바꾸지 않는다. 스크롤 한계에서 더 이동할
수 없는 입력은 상위 스크롤 영역으로 전달한다.
읽기 전용 여러 줄 텍스트는 처음 생성하거나 외부에서 내용을 교체할 때 커서와 내부
스크롤을 문서의 시작에 둔다. 같은 값이 다시 전달되는 경우에는 현재 선택을 유지한다.

텍스트·자리표시자·활성화·읽기 전용·너비·높이·최소 높이·여백·글꼴·접근성 이름은
명시하거나 모델에서 구독할 수 있다. 사용자 편집만 `onEdited`로 전달하며 외부 값
갱신은 편집 이벤트를 만들지 않는다. `onContentHeightChanged`는 텍스트의 측정된
높이를 최초 부착과 값 변경 시에만 알리며, 글꼴·문자·줄바꿈 너비의 변화에 반응한다.
이 값은 안쪽 여백을 제외하며 임시 입력기 조합을 포함한다. 앱은 측정값과 여백,
최소·최대 높이를 조합해 모델의 높이를 설정할 수 있다. C++에서는 스냅샷의
`content_height`를 질의할 수도 있다. 현재 입력 엔진에는 UTF-8 64 KiB와
줄 레이아웃 4096행 한도가 있다. 파일 로딩·저장, 서식 있는 텍스트와 자동 동기화는
이 계약에 포함되지 않으며 파일 소유권은 앱의 주입된 공급자가 담당한다.

```lvui
import vm NotesModel
Window {
    TextEditor(text: vm.text, placeholder: vm.placeholder, width: vm.width,
        height: vm.height, minimumHeight: vm.minimumHeight, onEdited: vm.edited,
        onContentHeightChanged: vm.measured)
}
```

`measured(double)`는 새 내용 높이를 받아 화면 모델의 높이를 갱신할 수 있다.
Python 컴파일러 API인 `ii_lvui.compile_native`도 이 선언을 처리한다.

운영체제 창을 띄운 검증이나 모든 플랫폼에서의 입력기 실기기 검증은 이 문서의
구현 범위를 뜻하지 않는다.

`LVUI/NativeTextEditor.h`와 설정 계약은 iiSDKCore 외 SDK를 직접 참조하지 않는다. macOS arm64에서 iiGUI 독립 설치 소비자와 LVUI 생성·반응형 높이·스크롤 경로를 창 없이 검사했다. 실제 운영체제 창과 GPU는 검사하지 않았다.
