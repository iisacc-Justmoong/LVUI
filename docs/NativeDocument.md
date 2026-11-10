# 문서 레이아웃과 앱 호스트 경계

파싱된 `lvui::Document`는 LVUI가 소유한다. 행·열·패딩·텍스트 배치, 초과 검사와 그리기 프레임 생성은 iiGUI의 `DocumentLayout.h`가 담당한다. `LVUI/NativeDocument.h`는 문서 구조를 전달하는 `DocumentLayoutPolicy`만 제공하며 iiSDKCore 외 SDK를 직접 참조하지 않는다. 앱이 두 모듈을 명시적으로 조합한다.

```cpp
#include <LVUI/NativeDocument.h>
#include <iiGUI/DocumentLayout.h>
auto document = lvui::parse("<view><p>Hello</p></view>");
using Policy = lvui::native::DocumentLayoutPolicy;
auto layout = ii::gui::layout_document<Policy>(*document, {640, 420});
// 앱의 Host가 열린 뒤 host.set_view(ii::gui::DocumentView<Policy>{*document})를 호출한다.
```

`DocumentView`는 소유하지 않는 참조이므로 제출 호출이 끝날 때까지 문서가 살아 있어야 한다. `layout_document`는 `CanvasFrame`과 ID가 있는 요소의 논리 경계를 반환한다. 유효하지 않은 후보는 기존 캔버스를 대체하지 않는다. 창 크기가 바뀌면 앱이 다시 제출하며 글자 높이는 폭에 따른 네이티브 글꼴 측정으로 계산한다.

이 경로는 파싱형 API이며 AOT 위젯 생성과 별개이다. macOS arm64에서 크기에 따른 배치, 글자 줄바꿈, 잘못된 크기, 독립 설치 소비자를 창 없이 검사했다. 실제 창과 GPU 검증은 이 결과에 포함되지 않는다.
