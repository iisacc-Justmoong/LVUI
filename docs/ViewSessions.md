# 뷰 세션의 값과 동작

> 2026-11-07 기준: 현재 채택된 언어 경계는 [뷰 전용 아키텍처](ViewOnlyArchitecture.md)이다.
> 앱 객체와 메서드는 C++에서 정의해 import한다. LVUI 객체 리터럴과 이름 붙인
> 스크립트 본문은 허용하지 않으며, 지역 변수·상수와 이벤트 블록은 허용한다.

뷰 세션은 선언이 속한 뷰의 활성화이다. 활성화마다 지역 상태와 소유 객체를
독립적으로 만들고, 다시 그릴 때는 같은 상태를 유지한다. 조건부 뷰가 제거되면
그 뷰의 상태·바인딩·콜백도 퇴역한다.

`const`는 재대입할 수 없는 값이고, `var`는 이벤트에서 변경할 수 있는 값이다.
비정적 `const`는 읽는 원본이 바뀌면 다시 계산하고 `@Static const`는 활성화 시의
값을 보존한다. `import name Type`은 해당 세션이 C++ 객체를 소유하며,
`import class Type`과 구성 매개변수는 호출자가 전달한 타입 지정 객체를 받는다.
공유·차용 및 const의 실제 의미는 [C++ 구성 입력](ObjectInputs.md)을 따른다.

구조화된 객체의 정의와 변경은 C++ 모델에 둔다. View는 import한 객체의 값을
읽고 명령을 연결한다.

```lvui
import model DocumentModel
Window {
    VStack {
        Label(text: model.title)
        PushButton(text: "+", onClick: model.increment)
    }
}
```

C++에서 제공한 구조화 값의 조회·소유권·검증은 해당 C++ API가 담당한다.
LVUI의 화면 상태는 별도의 `var`와 `const`으로 선언할 수 있다.

이벤트의 `{ 이름 = 식 }` 블록은 해당 어휘 범위의 `var`에 대입한다. 여러 문장은
줄바꿈으로 구분하며 순서대로 실행한다. `const`, 매개변수, C++ 멤버 및 존재하지
않는 이름에 대한 대입은 거부한다. 블록은 이벤트가 발생할 때만 실행되고 기존
콜백의 스레드·퇴역 경계와 바인딩 일괄 갱신을 따른다. 문장 사이에서 파생값을
최신 상태로 정리하므로 뒤 문장은 앞 문장의 변경을 읽는다. 블록은 `() -> Void`
같은 타입 지정 콜백 매개변수에도 직접 전달할 수 있으며,
전달한 위치의 어휘 범위를 보존한다. C++의 상태 변경은 해당 객체의 명시적인
메서드나 쿼리로 한다. `QueryConnection`의 `value`로 이름을 지정한 페이로드는 해당
연결의 이벤트 블록에서 읽을 수 있다. 다른 컨트롤의 임의 이벤트 인자를 이름으로
읽는 일반 구문은 아직 없으며,
문장 실행 중 오류가 나더라도 이미 완료된 앞선 변경을 되돌리지 않는다.
블록에서 `openScene`, `closeScene`, `openWindow`, `closeWindow`로 이름 있는
장면을 요청할 수 있다. 앞선 변경을 반영해 대상을 선택하고 뒤의 변경과 독립적으로
요청을 접수한다. 사용 조건은 [Scene 계약](Scenes.md)을 따른다.

`var heading: View { Label(text: value) }`는 SwiftUI의 계산된 View 프로퍼티처럼
하나의 뷰 구성을 정의한다. `heading()`은 정의 위치의 값을 포획하여 각 사용
위치에 위젯을 생성하며, 값 변경은 기존 바인딩을 통해 반영한다. 이 프로퍼티는
값 대입으로 교체할 수 있는 저장형 `var View`가 아니다.

`query(owner.port, owner.request())` 또는 `query(owner.queries(), owner.request())`는
공개된 iiSDK 타입 지정 `QueryPort`에
요청을 동기 전송하고 성공 응답을 사용한다. 포트는 세션이 소유하거나 안전하게
차용한 백엔드 또는 전달받은 C++ 객체의 직접 공개 멤버나 인자 없는 공개 접근자여야 한다.
접근자는 유효한 포트 참조를 반환해야 한다.
읽기 쿼리는 값 식에, 명령 쿼리는 이벤트
블록에 사용할 수 있다. 포트의 실행 영역 검사와 응답 타입은 Core가 소유한다.
전송 실패는 예외로 바인딩·콜백의 실패 경로에 전달되며 도메인 응답 안의 별도
오류를 성공으로 바꾸지 않는다. 포트만으로 임의 변경을 감지할 수 없으므로
처리기가 `Observable`을 읽거나 소유자가 변경을 발행해야 한다.
공유 소유 객체에서 포트를 선택하면 동기 쿼리가 반환될 때까지 선택한 객체를 유지한다.
원시 포인터와 참조의 유효성은 호출자가 보장하며, 외부 객체의 임의 일반 필드 변경은
자동으로 감지하지 않는다.
수신 영역이 UI 영역과 다른 포트에 동기 전송하면 Core가 `wrong_executor`로
거부한다. 비동기 `post`는 이 구문의 범위가 아니다. 타입 지정 다중 수신 알림은
별도의 [`QueryConnection`](QueryConnections.md)으로 UI 실행 영역에 전달한다.
요청·응답 구성과 도메인 오류 처리는 C++ 계약이 소유한다.

```lvui
import model DocumentModel
const title: String = query(model.readPort, model.titleRequest())
Window { VStack {
    Label(text: title)
    PushButton(text: "Save", onClick: { query(model.commandPort, model.saveRequest()) })
} }
```

버튼으로 지역 상태를 변경하는 전체 선언은
[ViewSessionCounter.lvui](../example/GettingStarted/ViewSessionCounter.lvui)이다.

화면에 표시할 String을 외부 번역 파일과 연결하려면 `localized("키")`를 사용한다.
앱의 언어나 문자열을 실행 중 변경하면 반응형 `const`와 위젯 입력이 갱신된다.
`@Static const`와 저장형 `var`는 기존 초기화 수명을 유지한다. 전체 파일 형식과
소유권은 [앱 문자열 저장소](Localization.md)를 따른다.
