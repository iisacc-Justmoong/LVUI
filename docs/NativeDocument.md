# 문서 레이아웃과 앱 호스트 경계

기존 HTML 형태의 `lvui::Document`는 LVUI 파서가 소유한다. 앱이 iiGUI 표시를
원하면 `LVUI/NativeDocument.h`를 포함하고 LVUI와 iiGUI를 각각 연결한다.
문서의 행·열·패딩·텍스트 배치, 초과 검사와 그림 프레임 생성은 LVUI 측에서
수행한다. iiGUI는 문서 타입을 모르며 네이티브 글꼴의 폭 제한 측정과 범용
캔버스 제출을 제공한다. 이 어댑터는 LVUI 코어에 호스트 라이브러리를 링크하지
않고 앱이 조합할 때만 사용한다.

```cpp
#include <LVUI/NativeDocument.h>
auto document = lvui::parse("<view><p>Hello</p></view>");
auto layout = lvui::native::layout_document(*document, {640, 420});
// 앱의 Host가 열린 뒤 host.set_view(*document)를 호출할 수 있다.
```

`layout_document`는 생성된 `CanvasFrame`과 ID가 있는 요소의 논리 경계를 반환한다.
`Host::set_view`는 특정 라이브러리를 지정하지 않는 확장점이며, 앱에 포함된
LVUI 어댑터가 `render_view`를 제공한다. 레이아웃을 먼저 검증하므로 유효하지
않은 후보를 제출할 때 기존 캔버스가 유지된다. 창 크기가 바뀌면 앱이 다시
제출한다. 텍스트 높이는 지정된 폭의 네이티브 글꼴 측정으로 계산한다.
이전 iiGUI의 `Host::set_view(lvui::Document, ...)` 이진 심볼에 링크된 앱은
어댑터 헤더를 포함하고 LVUI를 명시적으로 연결하여 다시 빌드해야 한다.

이 문서 경로는 기존 파싱형 API이다. 컴포넌트 언어의 기본 AOT 위젯 생성과는
별개이며 전체 컴포넌트의 독립 구현이나 실제 창·GPU 실행을 뜻하지 않는다.
iiGUI만 사용하는 앱은 LVUI 패키지를 찾거나 링크할 필요가 없다.
