# C++ 사용자 정의 뷰를 LVUI에 구성

NativeView는 iiGUI NativeViewSource가 만드는 네이티브 위젯 트리를 LVUI 배치 안에 넣는다.
일반 컨트롤은 LVUI에 선언하고, 캔버스·타임라인·프레임 표시 등 사용자 정의 영역의
그리기·입력·상태 처리는 C++가 소유한다. Qt 객체, QML 엔진, 실행 중 LVUI 소스 해석은
사용하지 않는다. iiGUI와 LVRS 프레임워크 양쪽에서 같은 접점을 사용한다.

```lvui
import vm WorkspaceViews
Window(width: 1280, height: 800) { VStack {
    Label(text: vm.title())
    NativeView(id: "canvas", source: vm.canvas, width: vm.width, height: 480)
    NativeView(id: "timeline", source: vm.timeline, height: 160)
} }
```

source는 C++ shared_ptr<ii::gui::NativeViewSource> 또는 변환 가능한 파생 객체 핸들이다.
문자열·런타임 컴포넌트 이름을 해석하지 않는다. source를 생략하면 원천을 기다리는 빈
호스트를 만든다. width·height·enabled·clip을 함께 선언할 수 있고 visible·opacity·flex의
공통 규칙도 따른다. 콘텐츠는 C++ 원천이 구성하므로 LVUI 자식 뷰는 직접 넣지 않는다.

일반 source와 옵션은 기존 네이티브 의존 그래프로 갱신한다. getter 안의 Core Observable과
스코프 revision, @Static의 고정 원천, 조건부·반복 활성화의 수명 규칙을 그대로 사용한다.
원천의 포인터 정체성이 같으면 내용을 다시 만들지 않는다. 새 원천의 후보가 실패하면
이전 위젯·입력·포커스를 유지하고 바인딩 오류를 기록한다. 다음 정상 후보로 복구할 수 있다.

원천은 뷰를 구성하는 제공자이며 영상의 개별 프레임이 아니다. 프레임마다 원천 객체를
교체하지 않는다. 유지되는 C++ 상태에서 기존 Widget의 set_image/set_drawing, paint/frame
handler를 갱신한다. 디코딩이 완료되기 전까지 마지막 정상 프레임을 유지하고, UI 적용은
기존 소유 루프를 사용한다. 스크롤·호버·플레이헤드 같은 뷰 상태와 파일 상태도 분리한다.

제공자는 빌더를 호출 이후까지 보관하지 않는다. 뷰별 자원은 retain/on_retire로 연결하며
부모 종료 후 늦은 UI 쓰기를 차단한다. NativeView 핸들은 약한 세대 접점이고 다른 활성화를
소유하거나 퇴역한 원천을 붙잡지 않는다. 원천이 일으킨 외부 파일·업무 부수 효과는 후보
교체의 롤백 범위에 포함하지 않는다.

실제 iiGUI 접점과 오류·수명 계약은 [NativeView](../../iiGUI/docs/NativeView.md)에 있다.
원본 감사는 NativeView의 iiGUI 구현 헤더를 별도로 해시하며, 이를 기존 LVRS Qt 선언의
구현 수에 합산하지 않는다. 생성 C++ 컴파일·실제 입력/그리기·설치 소비자 실행 결과는
개별 검사 기록으로 구분한다. 키워드 등록만으로 Dreamscapes 변환 준비 완료를 판단하지 않는다.
