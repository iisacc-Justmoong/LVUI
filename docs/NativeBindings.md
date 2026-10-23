# 네이티브 속성 바인딩

일반 C++ 속성 참조는 `ii::core::Observable<T>`에 연결한다. AOT는 초기 값을 직접 읽고
이후 변경을 iiGUI의 타입별 setter에 전달하는 C++ 구독을 생성한다. 런타임 소스·표현식
해석이나 속성 이름 레지스트리는 없다. 문자열 `text`, `enabled`, `checked`, `visible`,
수치 `value`/`currentValue`가 현재 연결 범위이다. 그 외 입력은 지원 계약을 별도로 확인한다.

```lvui
import vm Backend
var note: String = "Ready"
ApplicationWindow {
    VStack {
        Label(text: vm.title)
        Label(text: note)
        PushButton(vm.run, text: "Run", enabled: vm.enabled)
    }
}
```

`Backend::title`은 `Observable<std::string>`, `enabled`는 `Observable<bool>`로 선언한다.
일반 필드를 반응형처럼 한 번 읽고 무시하지 않는다. 타입 계약은 생성된 C++ 컴파일에서
검사한다. 스칼라 `var`는 생성 View 스코프의 Observable이며 `set_note()` 같은 타입 setter를
제공한다. `let alias = vm.title`은 같은 속성을 빌려 관측한다. 일반 상수 `let`은 값이 변하지 않는다.

바인딩 수명은 위젯 세대에 묶는다. 모달의 열림 단위 콘텐츠는 매번 별도의 BindingScope를
보관하고 닫을 때 구독을 해제한다. 부모 백엔드는 기존 View 스코프에서 재사용한다.
뷰가 닫힌 뒤 생성 View의 변수 setter는 `closed`를 반환한다. 작업자는 기존 Core 큐를 통해
소유 스레드에 갱신을 전달한다.

공통 `visible` 입력과 선형 스택의 `flex` 입력은 각 C++ 위젯의 기존 정책을 사용한다.
반응형 수식·임의 메서드 호출·동적 문법 해석을 추가한 것은 아니다.

백엔드 구성 콜백은 Scope의 소유권을 확보한 직후, 위젯을 생성하기 전에 실행된다. 따라서 ListModel 같은 공유 모델도 같은 인스턴스를 주입하여 초기 위젯 생성과 이후 변경 구독에 사용할 수 있다.
